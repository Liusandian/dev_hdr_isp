/**
 * @file raw_ltm.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: ltm (局部色调映射 / Local Tone Mapping) —— HDR 核心
// -----------------------------------------------------------------------------
// 功能: 将宽动态范围(HDR)图像压缩到显示设备可呈现的窄动态范围(SDR), 同时
//       保留局部对比度和细节, 避免全局映射导致的细节丢失和颜色失真。
//
// 算法原理(基于对数域的双边滤波分解, Durand-Dorsey 方法):
//   1) 亮度提取: 由 BGR 计算亮度 Y(加权平均), 并取对数 log10(Y)
//      —— 对数域更符合人眼对亮度的感知, 且将乘性关系转为加性关系
//   2) 基础层/细节层分解(核心): 用双边滤波对 log(Y) 滤波, 得到基础层 base
//      (大尺度光照信息, 含动态范围), 细节层 detail = log(Y) - base
//      (高频纹理, 动态范围小)
//      —— 双边滤波同时考虑空间距离(高斯)和值域差异(高斯), 能在平滑光照的
//         同时保留边缘, 避免普通高斯模糊导致的边缘晕影(halo)
//   3) 基础层对比度压缩: 对 base 乘以压缩因子 scale_factor
//      scale_factor = log(contrast) / (max_base - min_base)
//      使基础层动态范围按对比度参数压缩
//   4) 重建: log(Y') = scale_factor * base + detail, 再 10^x 还原线性
//   5) 增益应用: ratio = Y' / Y, 将该增益比例乘回原始 BGR 三通道
//      (保持色彩 hue 不变, 仅调整亮度)
//   6) 归一化输出: 按 max_out_val 缩放到目标位深
//
// 双边滤波核: final_kernel = gauss_space(空间) * gauss_range(值域)
//   空间核: exp(-0.5 * (dx^2 + dy^2) / space_sigma^2)
//   值域核: exp(-0.5 * (I_p - I_q)^2 / range_sigma^2)
//
// 数据流向: s32(BGR) -> s32(BGR), 色彩域保持 BGR。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "ltm"

// 双边滤波用全局核缓冲(避免每像素重复分配)
static float g_guass_kernel[kMaxLtmKenerlSize][kMaxLtmKenerlSize];   // 空间高斯核(仅与位置有关, 预计算一次)
static float g_range_kernel[kMaxLtmKenerlSize][kMaxLtmKenerlSize];   // 值域高斯核(与像素差有关, 每像素不同)
static float g_final_kernel[kMaxLtmKenerlSize][kMaxLtmKenerlSize];   // 最终联合核 = 空间核 * 值域核

// -----------------------------------------------------------------------------
// 快速 BGR -> 亮度 Y 转换 + 对数变换
// 输出: y(线性亮度), y_log(对数亮度 log10)
// 加权系数近似 BT.601 (B:G:R = 20:40:1 / 61), 简化为整数比避免浮点乘法
// -----------------------------------------------------------------------------
static void BgrToYFast(int32_t *bgr, float *y, float *y_log, int width, int height)
{
    int max_y = 0, min = 1000;

    FOR_ITER(ih, height)
    {
        FOR_ITER(iw, width)
        {
            int pixel_index = GET_PIXEL_INDEX(iw, ih, width);
            // 亮度 Y = (20*B + 40*G + 1*R) / 61, 近似 BT.601 加权
            y[pixel_index] = (bgr[pixel_index * 3 + 0] * 20 + bgr[pixel_index * 3 + 1] * 40 + bgr[pixel_index * 3 + 2]) / 61.0f;
            // 对数域表示: log10(Y), 将动态范围压缩到对数刻度
            y_log[pixel_index] = log10f(y[pixel_index]);
            if (y[pixel_index] > max_y)
            {
                max_y = y[pixel_index];
            }
            else if (y[pixel_index] < min)
            {
                min = y[pixel_index];
            }
        }
    }
}

// -----------------------------------------------------------------------------
// 双边滤波 (Bilateral Filter) —— LTM 核心
// -----------------------------------------------------------------------------
// 对每个像素, 在 kernel_size x kernel_size 邻域内加权平均:
//   权重 = 空间高斯(距离) * 值域高斯(亮度差)
//   空间高斯: 中心权重大, 远处权重小 -> 平滑作用
//   值域高斯: 亮度差小的权重高, 差异大(跨边缘)的权重低 -> 保边作用
// 两者相乘实现"平滑区域强滤波, 边缘处弱滤波", 比普通高斯更适合做基础层提取。
//
// 输出: bp_log (基础层 base, 对数域), 以及基础层最大/最小值(用于后续压缩)
// 边界处理: 采用边缘延伸(clamp) padding, 防止越界访问
// -----------------------------------------------------------------------------
static void BilateralFilter(float *ylog_i, float *bp_log, int kernel_size,
                            int width, int height, float space_sigma,
                            float range_sigma, float &log_base_max, float &log_base_min)
{
    log_base_max = 0;
    log_base_min = 100;

    if (kernel_size > kMaxLtmKenerlSize)
    {
        kernel_size = kMaxLtmKenerlSize;
    }
    // std::cout << kernel_size << "\n";
    int center = kernel_size >> 1;               // 核中心偏移
    float sigma_2 = space_sigma * space_sigma;   // 空间方差平方

    // 预计算空间高斯核(仅依赖位置, 全图共用)
    for (int kh = 0; kh < kernel_size; ++kh)
    {
        for (int kw = 0; kw < kernel_size; ++kw)
        {
            float exp_scale = -0.5f * ((kh - center) * (kh - center) + (kw - center) * (kw - center)) / sigma_2;
            g_guass_kernel[kw][kh] = exp(exp_scale);
            // printf("[%f]", g_guass_kernel[kw][kh]);
        }
        // printf("\n");
    }

    sigma_2 = range_sigma * range_sigma;   // 值域方差平方
    FOR_ITER(ih, height)
    {
        FOR_ITER(iw, width)
        {
            // 每个像素做filter
            float y_gray = 0;
            float filter_kernel_sum = 0;   // 权重归一化和
            float filter_result = 0;       // 加权累加结果
            int pixel_id = GET_PIXEL_INDEX(iw, ih, width);

            // 遍历核内邻域
            for (int kh = -center; kh <= center; ++kh)
            {
                for (int kw = -center; kw <= center; ++kw)
                {
                    int idx = iw + kw;
                    int idy = ih + kh;

                    // 边界 padding: 越界时用最近的边缘像素值(extend/clamp)
                    if ((idx < 0) && (idy < 0))
                    {
                        y_gray = ylog_i[0];   // 左上角
                    }
                    else if ((idx > 0) && (idy < 0))
                    {
                        if (idx < width)
                        {
                            y_gray = ylog_i[idx];
                        }
                        else
                        {
                            y_gray = ylog_i[width - 1];
                        }
                    }
                    else if ((idx < 0) && (idy > 0))
                    {
                        if (idy < height)
                        {
                            y_gray = ylog_i[idy * width];
                        }
                        else
                        {
                            y_gray = ylog_i[(height - 1) * width];
                        }
                    }
                    else if ((idx >= 0) && (idy >= 0))
                    {
                        if ((idx < width) && (idy < height))
                        {
                            y_gray = ylog_i[idx + idy * width];   // 正常内部像素
                        }
                        else if ((idx >= width) && (idy < height))
                        {
                            y_gray = ylog_i[(width - 1) + idy * width];   // 右边界
                        }
                        else if ((idx >= width) && (idy >= height))
                        {
                            y_gray = ylog_i[height * width - 1];   // 右下角
                        }
                        else if ((idx < width) && (idy >= height))
                        {
                            y_gray = ylog_i[(height - 1) * width + idx];   // 下边界
                        }
                        else
                        {
                            LOG(ERROR) << "error padding";
                            y_gray = 0;
                        }
                    }

                    // 值域高斯权重: 邻点与中心亮度差越小, 权重越大(保边关键)
                    float exp_scale = (-0.5 * (y_gray - ylog_i[pixel_id]) * (y_gray - ylog_i[pixel_id])) / sigma_2;
                    g_range_kernel[kw + center][kh + center] = exp(exp_scale);

                    // 联合核 = 空间核 * 值域核
                    g_final_kernel[kw + center][kh + center] = g_guass_kernel[kw + center][kh + center] * g_range_kernel[kw + center][kh + center];

                    // 加权累加
                    filter_result += (g_final_kernel[kw + center][kh + center] * y_gray);
                    filter_kernel_sum += g_final_kernel[kw + center][kh + center];
                }
            }
            // 归一化(除以权重和), 得到基础层
            filter_result = filter_result / filter_kernel_sum;
            bp_log[pixel_id] = filter_result;

            if (bp_log[pixel_id] < 0)
            {
                bp_log[pixel_id] = 0;
            }

            // 记录基础层动态范围(最大/最小值), 用于计算压缩因子
            if (filter_result > log_base_max)
            {
                log_base_max = filter_result;
            }
            else if (filter_result < log_base_min)
            {
                log_base_min = filter_result;
            }
        }
    }
}

// LTM 主处理函数
static int Ltm(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    const auto &ltm_prms = isp_prm->ltm_prms;   // LTM 参数(空间/值域 sigma, 对比度, 输出位深)

    int32_t *bgr_in = reinterpret_cast<int32_t *>(frame->data.bgr_s32_i);   // CCM 后 BGR 输入
    int32_t *bgr_out = reinterpret_cast<int32_t *>(frame->data.bgr_s32_o);  // 色调映射后 BGR 输出

    // 借用内存
    // 复用 YUV 浮点缓冲区存放中间结果, 避免额外分配
    float *y_log = reinterpret_cast<float *>(frame->data.yuv_f32_i.y);    // 对数亮度 log10(Y)
    float *bp_log = reinterpret_cast<float *>(frame->data.yuv_f32_i.u);   // 基础层 base (对数域)
    float *dp_log = reinterpret_cast<float *>(frame->data.yuv_f32_i.v);   // 细节层 detail (对数域)

    float *y_val = reinterpret_cast<float *>(frame->data.yuv_f32_o.y);        // 线性亮度 Y
    float *funstion_val = reinterpret_cast<float *>(frame->data.yuv_f32_o.u); // 重建后的 Y' (10^x 还原)

    float max_log_base = 0;     // 基础层最大值(对数)
    float min_log_base = 0;     // 基础层最小值(对数)

    // 步骤1: BGR -> Y(线性) + log10(Y)
    BgrToYFast(bgr_in, y_val, y_log, frame->info.width, frame->info.height);

    // 步骤2: 双边滤波分解, 得到基础层 bp_log (9x9 核)
    BilateralFilter(y_log, bp_log, 9, frame->info.width, frame->info.height,
                    ltm_prms.space_sigma, ltm_prms.range_sigma, max_log_base, min_log_base);

    // 步骤3: 计算细节层 detail = log(Y) - base (高频纹理)
    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);
            dp_log[pixel_idx] = y_log[pixel_idx] - bp_log[pixel_idx]; // detail layber
        }
    }

    // 步骤4: 计算基础层压缩因子
    //   scale_factor = log(contrast) / (max_base - min_base)
    //   将基础层动态范围 [min_base, max_base] 压缩到 [0, log(contrast)]
    float scale_factor = log10(ltm_prms.constrast) / (max_log_base - min_log_base);
    // 基础层最大值压缩后还原到线性域, 用于最终归一化
    float max_scale_val = (float)powf(10, scale_factor * max_log_base);

    float ratio = 0;
    int max_out_val = (1 << ltm_prms.out_bits) - 1;   // 输出最大值(由输出位深决定)
    // LOG(INFO) << "ltm ratio " << scale_factor;
    //  std::cout << "max_scale_val " << max_scale_val << "scale_factor " << scale_factor << "max log base " << max_log_base << " min "<< min_log_base<< "max_out_val " << max_out_val;
    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);

            // 步骤5: 重建压缩后的对数亮度 = scale_factor * base + detail
            //   基础层被压缩(乘 scale_factor), 细节层保持不变 -> 保留纹理细节
            y_log[pixel_idx] = scale_factor * bp_log[pixel_idx] + dp_log[pixel_idx];
            // 还原到线性域: Y' = 10^(压缩后 log)
            funstion_val[pixel_idx] = (float)powf(10, y_log[pixel_idx]);

            // 步骤6: 计算增益比例 ratio = Y' / Y, 乘回原始 BGR 三通道
            //   按比例调整亮度, 保持色度(hue)不变, 避免色彩偏移
            if ((y_val[pixel_idx] > 0) && (funstion_val[pixel_idx] > 0))
                ratio = funstion_val[pixel_idx] / y_val[pixel_idx];
            else
                ratio = 0;
            // std::cout << "ratio " << ratio << " | "<< funstion_val[pixel_idx] <<  " | "<< y_val[pixel_idx]  << "| " << bp_log[pixel_idx] << "|" << dp_log[pixel_idx]<< "\n";
            // LOG(INFO) << "ltm ratio " << ratio;
            bgr_in[3 * pixel_idx + 0] = static_cast<int32_t>(bgr_in[3 * pixel_idx + 0] * ratio);
            bgr_in[3 * pixel_idx + 1] = static_cast<int32_t>(bgr_in[3 * pixel_idx + 1] * ratio);
            bgr_in[3 * pixel_idx + 2] = static_cast<int32_t>(bgr_in[3 * pixel_idx + 2] * ratio);

            // 步骤7: 归一化到输出位深 [0, max_out_val]
            bgr_out[3 * pixel_idx + 0] = static_cast<int32_t>(max_out_val * bgr_in[3 * pixel_idx + 0] / max_scale_val);
            bgr_out[3 * pixel_idx + 1] = static_cast<int32_t>(max_out_val * bgr_in[3 * pixel_idx + 1] / max_scale_val);
            bgr_out[3 * pixel_idx + 2] = static_cast<int32_t>(max_out_val * bgr_in[3 * pixel_idx + 2] / max_scale_val);

            // 限幅
            ClipMinMax<int32_t>(bgr_out[3 * pixel_idx + 0], max_out_val, 0);
            ClipMinMax<int32_t>(bgr_out[3 * pixel_idx + 1], max_out_val, 0);
            ClipMinMax<int32_t>(bgr_out[3 * pixel_idx + 2], max_out_val, 0);
        }
    }

    SwapMem<void>(frame->data.bgr_s32_i, frame->data.bgr_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, BGR -> BGR
void RegisterLtmMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::BGR;
    mod.out_domain = ColorDomains::BGR;

    mod.name = MOD_NAME;
    mod.run_function = Ltm;

    RegisterIspModule(mod);
}