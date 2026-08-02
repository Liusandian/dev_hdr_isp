/**
 * @file cns.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief chrome noise signal filter
 * @version 0.1
 * @date 2023-08-27
 * Copyright (c) of ADAS_EYES 2023
 */
// =============================================================================
// 模块: cns (色度噪声抑制 / Chrome Noise Suppression)
// -----------------------------------------------------------------------------
// 功能: 对 YUV 的色度分量(U/V)做中值滤波, 去除彩色噪点, 同时保留边缘。
//
// 背景: 弱光环境下, 传感器噪声经白平衡、去马赛克放大后, 在平坦区域表现为
//       彩色斑点(色度噪声)。由于人眼对色度噪声更敏感, 需专门滤除。中值滤波
//       对孤立噪点(脉冲噪声)去除效果好, 且能保留边缘(不像均值滤波那样模糊)。
//
// 算法(3x3 中值滤波, 分别作用于 U 和 V 平面):
//   1) 取当前像素 3x3 邻域 9 个色度值
//   2) 排序后取中值(第 5 个, 即 filter_center = 9/2 = 4)作为输出
// 中值滤波对椒盐噪声/孤立色点去除效果最优, 不会平均化边缘。
// 数据流向: u8(U/V) -> u8(U/V), 色彩域保持 YUV(仅滤色度, 不动亮度)。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "cns"

// 色度噪声抑制主函数
static int Cns(Frame *frame, const IspPrms *isp_prm)
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


    constexpr int boundary_pixel = 1;                                    // 边界宽度(3x3 核需 1 像素边界)
    constexpr int filter_size = (2 * boundary_pixel + 1) * (2 * boundary_pixel + 1);  // 9 = 3x3
    constexpr int filter_center = filter_size >> 1;                      // 中值索引 = 4 (排序后第5个)
    uint8_t u[filter_size];   // U 邻域缓存
    uint8_t v[filter_size];   // V 邻域缓存

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            int pixel_idx = ih * frame->info.width + iw;
            // 边界像素不处理, 直接拷贝
            if ((iw < boundary_pixel) || (iw >= (frame->info.width - boundary_pixel)) || (ih < boundary_pixel) || (ih >= (frame->info.height - boundary_pixel))) {
                u_o[pixel_idx] = u_i[pixel_idx];
                v_o[pixel_idx] = v_i[pixel_idx];
                continue;
            }

            // 收集 3x3 邻域的 9 个色度值
            int sub_index = 0;
            for (int idy = -boundary_pixel; idy <= boundary_pixel; ++idy) {
                for (int idx = -boundary_pixel; idx <= boundary_pixel; ++idx) {
                    int filer_pixel_idx = GET_PIXEL_INDEX((iw + idx), (ih + idy), frame->info.width);
                    u[sub_index] = u_i[filer_pixel_idx];
                    v[sub_index] = v_i[filer_pixel_idx];
                    ++sub_index;
                }
            }
            //meida filter
            // 中值滤波: 排序后取中值(第 filter_center 个)
            std::sort(u, u + filter_size);
            std::sort(v, v + filter_size);

            u_o[pixel_idx] = u[filter_center];   // U 中值
            v_o[pixel_idx] = v[filter_center];   // V 中值
        }
    }

    SwapMem<void>(frame->data.yuv_u8_i.u, frame->data.yuv_u8_o.u);
    SwapMem<void>(frame->data.yuv_u8_i.v, frame->data.yuv_u8_o.v);

    return 0;
}

// 模块注册: s32 -> s32, YUV -> YUV
void RegisterCnsMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::YUV;
    mod.out_domain = ColorDomains::YUV;

    mod.name = MOD_NAME;
    mod.run_function = Cns;

    RegisterIspModule(mod);
}