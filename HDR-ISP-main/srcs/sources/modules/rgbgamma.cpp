/**
 * @file rgbgamma.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-08-10
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: rgbgamma (RGB 伽马校正)
// -----------------------------------------------------------------------------
// 功能: 对 BGR 三通道分别施加伽马曲线变换, 调整图像色调响应, 适配显示设备
//       的非线性响应特性, 或实现艺术化色调风格。
//
// 背景: 显示器对输入电压的响应是非线性的(近似 y = x^gamma)。为使图像在显示
//       时呈现线性感知, 需在输出前进行反向伽马补偿(如 sRGB 标准用 gamma=2.2)。
//       也可用于 HDR 后处理, 将 LTM 输出的线性值映射到显示空间。
//
// 算法: 通过预先生成的伽马查找表(LUT, curve[nums])实现, 避免每像素计算 pow。
//   1) 将输入像素值映射到 LUT 索引: idx = value * (nums-1) / (2^in_bits)
//   2) 在相邻 LUT 节点间线性插值: scale = (idx-idx_floor)*(curve[i+1]-curve[i]) + curve[i]
//   3) 还原到输出位深: out = scale * (2^out_bits - 1)
// 数据流向: s32(BGR) -> s32(BGR), 色彩域保持 BGR。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "rgbgamma"

// RGB 伽马校正主函数
static int RgbGamma(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    const auto &gamma_prm = isp_prm->rgb_gamma;   // 伽马曲线参数(查找表 + 输入/输出位深)

    // LUT 索引步长: 将输入值[0, 2^in_bits]映射到 LUT[0, nums-1]
    float step_coff = (float)(gamma_prm.nums - 1) / (1 << gamma_prm.in_bits);
    // 输出最大值(由输出位深决定)
    float out_max = (1 << gamma_prm.out_bits) - 1;

    int32_t *bgr_i = reinterpret_cast<int32_t *>(frame->data.bgr_s32_i);   // LTM 后 BGR 输入
    int32_t *bgr_o = reinterpret_cast<int32_t *>(frame->data.bgr_s32_o);   // 伽马校正后输出

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;

            // 对 B/G/R 三通道分别做伽马变换
            FOR_ITER(color_idx, 3)
            {
                auto color = bgr_i[3 * pixel_idx + color_idx];

                // 计算伽马曲线上的浮点索引
                float cuvre_id_f = color * step_coff;
                int curve_id = static_cast<int>(cuvre_id_f);   // 整数部分(区间下界)
                // scale to 0~1
                // 在 LUT 相邻节点间线性插值, 得到归一化输出值(0~1)
                float scale = (cuvre_id_f - curve_id) * (gamma_prm.curve[curve_id + 1] - gamma_prm.curve[curve_id]) + gamma_prm.curve[curve_id];
                // get scale value
                // 还原到输出位深
                color = out_max * scale;
                // ClipMinMax(color, isp_prm->info.max_val, 0);
                bgr_o[3 * pixel_idx + color_idx] = color;
            }
        }
    }

    SwapMem<void>(frame->data.bgr_s32_i, frame->data.bgr_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, BGR -> BGR
void RegisterRgbGammaMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::BGR;
    mod.out_domain = ColorDomains::BGR;

    mod.name = MOD_NAME;
    mod.run_function = RgbGamma;

    RegisterIspModule(mod);
}