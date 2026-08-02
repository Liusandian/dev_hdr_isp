/**
 * @file lsc.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: lsc (镜头阴影校正 / Lens Shading Correction)
// -----------------------------------------------------------------------------
// 功能: 补偿因镜头光学特性导致的图像四周变暗(暗角)及偏色现象。
//
// 背景: 镜头边缘入射光线存在衰减(cos4 定律), 加上彩色滤光片的光谱响应随
//       角度变化, 导致图像四角比中心暗且偏色(颜色不均匀)。LSC 通过对每个像素
//       乘以一个空间位置相关的增益来补偿: 增益在中心约为 1, 四角大于 1。
//
// 实现: 采用网格(Mesh) + 双线性插值:
//   1) 将图像划分为 kLscMeshBoxHNums x kLscMeshBoxVNums 个网格
//   2) 每个网格顶点存储 R/Gr/Gb/B 四通道的增益标定值
//   3) 像素所在网格内, 用双线性插值计算其精确增益
//   4) 按像素 CFA 类型选择对应通道增益, 乘到原始值上
//
// 双线性插值公式(4 个顶点 gain0..gain3, 归一化坐标 x_coff, y_coff):
//   gain = (gain0*x + gain1*(1-x))*y + (gain2*x + gain3*(1-x))*(1-y)
// 数据流向: s32 -> s32 (色彩域保持 RAW)。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "lsc"
/*
*gain0         *gain1

      . x_coff,y_coff


*gain2         *gain3

x_coff 0~1
y_coff 0~1
*/
// 双线性插值: 在网格 4 个顶点增益之间插值得到当前像素的增益
//   gain0/1 为上排左右, gain2/3 为下排左右; x_coff/y_coff 为像素在网格内的归一化坐标(0~1)
static inline float BilinearInterpolation(float gain0, float gain1, float gain2, float gain3, float x_coff, float y_coff)
{
    if (x_coff > 1 || y_coff > 1) {
        return 1;
    }

    // 上排水平插值
    float gain01 = gain0 * x_coff + gain1 * (1 - x_coff);
    // 下排水平插值
    float gain23 = gain2 * x_coff + gain3 * (1 - x_coff);

    // 垂直方向再插值得到最终增益
    return gain01 * y_coff + gain23 * (1 - y_coff);
}


// LSC 主处理函数
static int Lsc(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    int pwl_idx = 0;

    int32_t* raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);   // BLC 后 RAW 输入
    int32_t* raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);  // LSC 校正后输出

    const LscPrms *lsc_prm = &(isp_prm->lsc_prms);  // 网格增益表(R/Gr/Gb/B 四通道)

    // 计算每个网格的宽高(像素数), 向上取整
    const int mesh_box_width =  static_cast<int>(ceil((float)frame->info.width / kLscMeshBoxHNums)) ;
    const int mesh_box_height =  static_cast<int>(ceil((float)frame->info.height / kLscMeshBoxVNums));

    //std::cout << mesh_box_width << "|" << mesh_box_height;

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);
            // 当前像素所属网格索引(行列)
            int box_idx =  iw / mesh_box_width;
            int box_idy =  ih / mesh_box_height;
            // 像素在网格内的归一化坐标(0~1)
            float x_coff = (float)(iw % mesh_box_width) / mesh_box_width;
            float y_coff = (float)(ih % mesh_box_height) / mesh_box_height;
            int cfa_id = static_cast<int>(frame->info.cfa);
            float gain = 1;

            // 根据像素 CFA 类型(R/Gr/Gb/B)选择对应通道的 4 个网格顶点增益做双线性插值
            switch (kPixelCfaLut[cfa_id][iw % 2][ih % 2])
            {
            case PixelCfaTypes::R:
                gain = BilinearInterpolation(lsc_prm->mesh_r[box_idy][box_idx], lsc_prm->mesh_r[box_idy][box_idx + 1],
                                             lsc_prm->mesh_r[box_idy + 1][box_idx], lsc_prm->mesh_r[box_idy + 1][box_idx + 1],
                                             x_coff, y_coff);
                break;
            case PixelCfaTypes::GR:
                gain = BilinearInterpolation(lsc_prm->mesh_gr[box_idy][box_idx], lsc_prm->mesh_gr[box_idy][box_idx + 1],
                                             lsc_prm->mesh_gr[box_idy + 1][box_idx], lsc_prm->mesh_gr[box_idy + 1][box_idx + 1],
                                             x_coff, y_coff);
                break;
            case PixelCfaTypes::GB:
                gain = BilinearInterpolation(lsc_prm->mesh_gb[box_idy][box_idx], lsc_prm->mesh_gb[box_idy][box_idx + 1],
                                             lsc_prm->mesh_gb[box_idy + 1][box_idx], lsc_prm->mesh_gb[box_idy + 1][box_idx + 1],
                                             x_coff, y_coff);
                break;
            case PixelCfaTypes::B:
                gain = BilinearInterpolation(lsc_prm->mesh_b[box_idy][box_idx], lsc_prm->mesh_b[box_idy][box_idx + 1],
                                             lsc_prm->mesh_b[box_idy + 1][box_idx], lsc_prm->mesh_b[box_idy + 1][box_idx + 1],
                                             x_coff, y_coff);
                break;
            default:
                break;
            }

            // 将增益乘到原始像素值上, 补偿镜头衰减
            raw32_out[pixel_idx] = static_cast<int>(raw32_in[pixel_idx] * gain);

            ClipMinMax<int32_t>(raw32_out[pixel_idx], (int32_t)isp_prm->info.max_val, 0);
        }
    }

    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, RAW -> RAW
void RegisterLscMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::RAW;

    mod.name = MOD_NAME;
    mod.run_function = Lsc;

    RegisterIspModule(mod);
}