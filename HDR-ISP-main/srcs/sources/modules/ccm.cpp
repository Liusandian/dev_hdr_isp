/**
 * @file ccm.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief color correct matrix
 * @version 0.1
 * @date 2023-07-27
 * Copyright (c) of ADAS_EYES 2023
 */
// =============================================================================
// 模块: ccm (色彩校正矩阵 / Color Correction Matrix)
// -----------------------------------------------------------------------------
// 功能: 通过 3x3 矩阵变换, 将传感器色彩空间映射到标准色彩空间(如 sRGB),
//       校正传感器滤光片光谱响应与标准观察者的偏差, 提升色彩还原准确性。
//
// 背景: 传感器 R/G/B 滤光片的光谱灵敏度与人眼/标准色彩空间不一致, 导致拍出的
//       颜色与真实颜色存在偏差(如偏色、饱和度异常)。CCM 通过线性变换:
//         [R']   [ccm[0][0] ccm[0][1] ccm[0][2]] [R]
//         [G'] = [ccm[1][0] ccm[1][1] ccm[1][2]] [G]
//         [B']   [ccm[2][0] ccm[2][1] ccm[2][2]] [B]
//       将传感器 RGB 映射到目标色彩空间。矩阵通常通过 ColorChecker 标定得到,
//       对角线元素接近 1, 非对角元素用于混色校正。
//
// 数据流向: s32(BGR) -> s32(BGR), 色彩域保持 BGR。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "ccm"

// 色彩校正主函数
static int Ccm(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    int32_t *bgr_in = reinterpret_cast<int32_t *>(frame->data.bgr_s32_i);   // 去马赛克后 BGR 输入
    int32_t *bgr_out = reinterpret_cast<int32_t *>(frame->data.bgr_s32_o);  // 校正后 BGR 输出

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;

            // 取出当前像素 BGR 分量(注意存储顺序为 B/G/R)
            int32_t _r = bgr_in[pixel_idx * 3 + 2];
            int32_t _g = bgr_in[pixel_idx * 3 + 1];
            int32_t _b = bgr_in[pixel_idx * 3 + 0];

            // 3x3 矩阵乘法: R'/G'/B' = CCM * [R, G, B]^T
            // ccm[i][0]*R + ccm[i][1]*G + ccm[i][2]*B, i=0,1,2 对应 R'/G'/B'
            bgr_out[pixel_idx * 3 + 2] = (int32_t)(_r * isp_prm->ccm_prms.ccm[0][0] + _g * isp_prm->ccm_prms.ccm[0][1] + _b * isp_prm->ccm_prms.ccm[0][2]);
            bgr_out[pixel_idx * 3 + 1] = (int32_t)(_r * isp_prm->ccm_prms.ccm[1][0] + _g * isp_prm->ccm_prms.ccm[1][1] + _b * isp_prm->ccm_prms.ccm[1][2]);bgr_out[pixel_idx * 3 + 0] = (int32_t)(_r * isp_prm->ccm_prms.ccm[2][0] + _g * isp_prm->ccm_prms.ccm[2][1] + _b * isp_prm->ccm_prms.ccm[2][2]);

            // 限幅到 [0, max_val]
            ClipMinMax<int32_t>(bgr_out[pixel_idx * 3 + 0], (int32_t)isp_prm->info.max_val, 0);
            ClipMinMax<int32_t>(bgr_out[pixel_idx * 3 + 1], (int32_t)isp_prm->info.max_val, 0);
            ClipMinMax<int32_t>(bgr_out[pixel_idx * 3 + 2], (int32_t)isp_prm->info.max_val, 0);
        }
    }

    SwapMem<void>(frame->data.bgr_s32_i, frame->data.bgr_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, BGR -> BGR
void RegisterCcmMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::BGR;
    mod.out_domain = ColorDomains::BGR;

    mod.name = MOD_NAME;
    mod.run_function = Ccm;

    RegisterIspModule(mod);
}