/**
 * @file ygamma.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: ygamma (亮度伽马校正)
// -----------------------------------------------------------------------------
// 功能: 仅对 YUV 的 Y(亮度)分量施加伽马曲线, 调整图像明暗分布, 不影响色度。
//
// 背景: 与 RGB 伽马不同, 亮度伽马只改变灰度映射, 不改变色彩, 适合在 YUV 域
//       单独调整曝光/对比度感知。常用于:
//       - sRGB 显示补偿(反向 gamma)
//       - HDR 到 SDR 的全局色调调整
//       - 提升暗部细节(曲线暗部斜率 > 1)
//
// 算法: 查找表(LUT) + 线性插值, 与 rgbgamma 模块相同, 仅作用于 Y 通道。
// 数据流向: u8(Y) -> u8(Y), 色彩域保持 YUV。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "ygamma"

// 亮度伽马校正主函数
static int YGamma(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    uint8_t *y_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.y);   // 输入亮度 Y
    uint8_t *y_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.y);   // 输出亮度 Y

    const auto &gamma_prm = isp_prm->y_gamma;   // 亮度伽马曲线参数

    // LUT 索引步长 + 输出最大值
    float step_coff = (float)(gamma_prm.nums - 1) / (1 << gamma_prm.in_bits);
    float out_max = (1 << gamma_prm.out_bits) - 1;

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;
            auto y = y_i[pixel_idx];

            // LUT 浮点索引 + 线性插值
            float cuvre_id_f = y * step_coff;
            int curve_id = static_cast<int>(cuvre_id_f);
            // scale to 0~1
            float scale = (cuvre_id_f - curve_id) * (gamma_prm.curve[curve_id + 1] - gamma_prm.curve[curve_id]) + gamma_prm.curve[curve_id];
            // get scale value
            y = out_max * scale;
            y_o[pixel_idx] = y;
        }
    }

    SwapMem<void>(frame->data.yuv_u8_i.y, frame->data.yuv_u8_o.y);

    return 0;
}

// 模块注册: s32 -> s32, YUV -> YUV
void RegisterYGammaMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::YUV;
    mod.out_domain = ColorDomains::YUV;

    mod.name = MOD_NAME;
    mod.run_function = YGamma;

    RegisterIspModule(mod);
}