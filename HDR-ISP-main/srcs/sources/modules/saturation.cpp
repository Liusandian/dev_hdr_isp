/**
 * @file saturation.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: saturation (饱和度调整)
// -----------------------------------------------------------------------------
// 功能: 调整 YUV 色度分量(U/V)的强度, 改变颜色鲜艳程度, 不影响亮度。
//
// 算法原理(UV 向量旋转):
//   在 YUV 空间中, 以 128(色度中心, 即灰色)为原点, (U-128, V-128) 构成色度向量。
//   饱和度 = 该向量的长度 |(U-128, V-128)|。通过对 UV 向量施加旋转矩阵,
//   改变其长度(类似 HSV 空间的 S 通道缩放):
//     [u']   [cos  sin] [u-128]       [128]
//     [v'] = [sin  cos] [v-128]   +   [128]
//   rotate_angle 控制旋转角度(实际控制饱和度增减):
//     - angle = 0:   无变化
//     - angle > 0:   增加饱和度(向量被放大)
//     - angle < 0:   降低饱和度(向量被缩小)
//   注: 此处公式为近似实现, 真正的饱和度调整通常是 UV 缩放而非旋转,
//       但旋转形式在特定参数下也能达到调饱和度的效果。
// 数据流向: u8(U/V) -> u8(U/V), 色彩域保持 YUV。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "saturation"

// 饱和度调整主函数
static int Saturation(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    uint8_t *u_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.u);   // 输入色度 U
    uint8_t *v_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.v);   // 输入色度 V

    uint8_t *u_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.u);   // 输出色度 U
    uint8_t *v_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.v);   // 输出色度 V

    // 旋转角度(度 -> 弧度), 计算正余弦
    float angle = 3.1415926f * isp_prm->sat_prms.rotate_angle / 180.0f;
    float sin_val = sin(angle);
    float cos_val = cos(angle);

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);

            // 将色度中心平移到 0(灰色对应 0,0)
            int u_tmp = u_i[pixel_idx] - 128;
            int v_tmp = v_i[pixel_idx] - 128;

            // UV 向量旋转: [u';v'] = [[cos,sin],[sin,cos]] * [u;v] + [128;128]
            int u = static_cast<uint8_t>(u_tmp * cos_val + v_tmp * sin_val + 128);
            int v = static_cast<uint8_t>(v_tmp * cos_val + u_tmp * sin_val + 128);

            // 限幅到 [0, 255]
            ClipMinMax(u, 255, 0);
            ClipMinMax(v, 255, 0);

            u_o[pixel_idx] = u;
            v_o[pixel_idx] = v;
        }
    }

    SwapMem<void>(frame->data.yuv_u8_o.u, frame->data.yuv_u8_i.u);
    SwapMem<void>(frame->data.yuv_u8_o.v, frame->data.yuv_u8_i.v);

    return 0;
}

// 模块注册: s32 -> s32, YUV -> YUV
void RegisterSaturationMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::YUV;
    mod.out_domain = ColorDomains::YUV;

    mod.name = MOD_NAME;
    mod.run_function = Saturation;

    RegisterIspModule(mod);
}