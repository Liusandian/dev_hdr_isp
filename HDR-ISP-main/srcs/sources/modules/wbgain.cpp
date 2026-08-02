/**
 * @file wb_gain.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: wbgain (白平衡增益 / White Balance Gain)
// -----------------------------------------------------------------------------
// 功能: 对 Bayer RAW 各通道施加不同增益, 使白色物体在成像后仍为白色(R=G=B),
//       消除光源色温造成的偏色。
//
// 背景: 不同色温光源(如 D65 日光 ~6500K, A 灯泡 ~2856K)光谱组成不同, 传感器
//       R/Gr/Gb/B 四通道响应不一致, 导致白色物体拍出偏蓝或偏黄。白平衡通过给
//       各通道乘以增益(通常以 G 为基准, 调整 R 和 B)来校正。
//
// 算法: 根据 CFA 类型查找像素所属通道, 乘以对应增益。
//   - r_gain  : R 通道增益
//   - gr_gain : Gr 通道增益(通常为 1)
//   - gb_gain : Gb 通道增益(通常为 1)
//   - b_gain  : B 通道增益
// 增益由 AWB(自动白平衡)算法估算, 或按光源色温查表(d65_gain 为 D65 光源)。
// 数据流向: s32 -> s32 (色彩域保持 RAW)。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "wbgain"

// 白平衡增益主函数
static int WbGain(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    int pwl_idx = 0;

    int32_t *raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);   // DPC 后 RAW 输入
    int32_t *raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);  // 白平衡后输出

    // default d65
    // 默认使用 D65(标准日光 ~6500K) 色温下的增益
    float r_gain = isp_prm->wb_gains.d65_gain[0];   // R 通道增益
    float gr_gain = isp_prm->wb_gains.d65_gain[1];  // Gr 通道增益
    float gb_gain = isp_prm->wb_gains.d65_gain[2];  // Gb 通道增益
    float b_gain = isp_prm->wb_gains.d65_gain[3];   // B 通道增益

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;

            // 根据像素 (w%2, h%2) 在 CFA 查找表中的类型选择对应通道增益
            int cfa_id = static_cast<int>(frame->info.cfa);
            switch (kPixelCfaLut[cfa_id][w % 2][h % 2])
            {
            case PixelCfaTypes::R:
                raw32_out[pixel_idx] = (int32_t)(raw32_in[pixel_idx] * r_gain);
                break;
            case PixelCfaTypes::GR:
                raw32_out[pixel_idx] = (int32_t)(raw32_in[pixel_idx] * gr_gain);
                break;
            case PixelCfaTypes::GB:
                raw32_out[pixel_idx] = (int32_t)(raw32_in[pixel_idx] * gb_gain);
                break;
            case PixelCfaTypes::B:
                raw32_out[pixel_idx] = (int32_t)(raw32_in[pixel_idx] * b_gain);
                break;
            default:
                break;
            }

            ClipMinMax<int32_t>(raw32_out[pixel_idx], (int32_t)isp_prm->info.max_val, 0);
        }
    }

    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, RAW -> RAW
void RegisterWbGaincMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::RAW;

    mod.name = MOD_NAME;
    mod.run_function = WbGain;

    RegisterIspModule(mod);
}