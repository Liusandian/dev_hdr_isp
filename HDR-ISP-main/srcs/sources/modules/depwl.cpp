/**
 * @file depwl.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief decode pwl curve
 * @version 0.1
 * @date 2023-07-27
 * Copyright (c) of ADAS_EYES 2023
 */
// =============================================================================
// 模块: depwl (HDR 分段线性解压缩 / De-Piecewise-Linear)
// -----------------------------------------------------------------------------
// 功能: 对 HDR 传感器输出的压缩 RAW 数据进行分段线性(PWL)反变换, 恢复
//       线性域的宽动态范围像素值。
//
// 背景: 为在有限位宽(如 12bit/14bit)内表达更高的动态范围, HDR 传感器在
//       模数转换前对光电响应做分段线性压缩。压缩曲线由若干折线段构成,
//       每段有不同的斜率(高光段斜率小, 暗部段斜率大), 从而提升高光信息密度。
//
// 解压公式(逐像素):
//   1) 根据输入 x 在 PWL 横坐标 x_cood[] 中定位所属区间 [x_{n-1}, x_n]
//   2) 线性插值还原: y = slope[n] * (x - x_{n-1}) + y_{n-1}
// 输出为 int32 以容纳解压后的宽动态范围(位宽可能 > 16bit)。
// 数据流向: u16 输入 -> s32 输出 (色彩域保持 RAW)。
// =============================================================================
#include "modules/modules.h"

#define MOD_NAME "depwl"

// 分段线性解压主函数
static int Depwl(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    int pwl_idx = 0;

    uint16_t* raw16_in = reinterpret_cast<uint16_t *>(frame->data.raw_u16_i);   // 压缩 RAW 输入
    int32_t*  raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);   // 解压后宽动态输出

    const DePwlPrms *pwl_prm = &(isp_prm->depwl_prm);  // PWL 折线参数(横纵坐标 + 斜率)

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;
            // 定位当前像素值所属 PWL 区间: 找到第一个 x_cood[index] >= raw 的折点
            for(int index = 1; index < pwl_prm->pwl_nums; ++index){
                if (raw16_in[pixel_idx] <= pwl_prm->x_cood[index]) {
                    pwl_idx = index;
                    break;
                }
                pwl_idx = index;
            }
            if (pwl_idx == 0) {
                return -1;
            }
            //y = slope * (Xn - Xn-1) + Yn-1
            // 线性还原: 在区间 [x_{n-1}, x_n] 内按斜率 slope[n] 插值
            //   y = slope[n] * (x - x_{n-1}) + y_{n-1}
            raw32_out[pixel_idx] = (int32_t)((raw16_in[pixel_idx] - pwl_prm->x_cood[pwl_idx - 1]) * pwl_prm->slope[pwl_idx] \
                                 + pwl_prm->y_cood[pwl_idx - 1]);

            // 限幅到 [0, max_val], 防止数值溢出
            ClipMinMax(raw32_out[pixel_idx], isp_prm->info.max_val, 0);
        }
    }

    // 交换缓冲, 使解压结果成为下一模块输入
    SwapMem<void>(frame->data.raw_s32_o, frame->data.raw_s32_i);

    return 0;
}

// 模块注册: u16 -> s32, RAW -> RAW
void RegisterDePwlMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_UINT16;
    mod.out_type = DataPtrTypes::TYPE_INT32;


    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::RAW;

    mod.name = MOD_NAME;
    mod.run_function = Depwl;

    RegisterIspModule(mod);
}