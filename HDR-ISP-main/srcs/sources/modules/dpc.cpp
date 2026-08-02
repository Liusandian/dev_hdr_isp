/**
 * @file dpc.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: dpc (坏点校正 / Defective Pixel Correction)
// -----------------------------------------------------------------------------
// 功能: 检测并校正传感器上的坏点(Dead/Hot/Stuck pixel)。
//
// 背景: CMOS 传感器制造工艺缺陷会产生异常像素: 暗电流过大(热噪点, 始终偏亮)、
//       响应过低(死点, 始终偏暗)或卡死在固定值。坏点若不校正会在后续去马赛克、
//       锐化等模块中被放大, 形成彩色伪彩斑。
//
// 检测算法(基于邻域阈值比较):
//   取中心像素 p0 的 5x5 邻域内 8 个采样点(间隔 2 像素, 同 CFA 通道),
//   若 p0 与所有 8 个邻点的差值均超过阈值 thres, 则判定为坏点。
//
// 校正算法(两种模式):
//   1) MEAN 模式: 用上下左右 4 个邻点(p2/p4/p5/p7)的均值替代
//   2) GRADIENT 模式(梯度方向选择): 计算垂直/水平/左对角/右对角 4 个方向的
//      二阶梯度 |2*p0 - p_a - p_b|, 取最小梯度方向(最平滑方向)的两个邻点均值
//      替代坏点。该方向最不可能包含真实边缘, 替代后伪影最小。
//
// 邻域布局(每 2 像素采样, 保证同 Bayer 通道):
//    p1  x  p2  x  p3
//    x   x  x   x  x
//    p4  x  p0  x  p5
//    x   x  x   x  x
//    p6  x  p7  x  p8
// 数据流向: s32 -> s32 (色彩域保持 RAW)。
// =============================================================================


#include "modules/modules.h"

#define MOD_NAME "dpc"

// 坏点校正主函数
static int Dpc(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    int pwl_idx = 0;

    int32_t* raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);   // LSC 后 RAW 输入
    int32_t* raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);  // 校正后输出

    int thres = isp_prm->dpc_prms.thres;   // 坏点判定阈值: 与邻点差值超过该值则判为坏点
    auto mode = isp_prm->dpc_prms.mode;     // 校正模式: MEAN(均值) / GRADIENT(梯度方向)

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {

            // 跳过边缘 2 像素(5x5 邻域越界), 边缘像素不处理
            if ((iw < 2) || (iw >= (frame->info.width - 2)) || (ih < 2) || (ih >= (frame->info.height - 2))) {
                continue;
            }

            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);

            /*
             p1 x  p2 x  p3
             x  x  x  x  x
             p4 x  p0 x  p5
             x  x  x  x  x
             p6 x  p7 x  p8
            */
            // 采样 5x5 邻域中 9 个同 CFA 通道点(间隔 2, 保证 Bayer 一致)
           int p0 = raw32_in[GET_PIXEL_INDEX(iw      , ih      , frame->info.width)];  // 中心像素
           int p1 = raw32_in[GET_PIXEL_INDEX((iw - 2), (ih - 2), frame->info.width)];  // 左上
           int p2 = raw32_in[GET_PIXEL_INDEX(iw      , (ih - 2), frame->info.width)];  // 正上
           int p3 = raw32_in[GET_PIXEL_INDEX((iw + 2), (ih - 2), frame->info.width)];  // 右上
           int p4 = raw32_in[GET_PIXEL_INDEX((iw - 2), ih      , frame->info.width)];  // 正左
           int p5 = raw32_in[GET_PIXEL_INDEX((iw + 2), ih      , frame->info.width)];  // 正右
           int p6 = raw32_in[GET_PIXEL_INDEX((iw - 2), (ih + 2), frame->info.width)];  // 左下
           int p7 = raw32_in[GET_PIXEL_INDEX(iw      , (ih + 2), frame->info.width)];  // 正下
           int p8 = raw32_in[GET_PIXEL_INDEX((iw + 2), (ih + 2), frame->info.width)];  // 右下

            // 坏点判定: 中心像素与所有 8 个邻点的差值均超过阈值, 才认为是坏点
            // (避免误检真实边缘: 边缘通常只有部分方向差异大)
            if ((abs(p1 - p0) > thres) && (abs(p2 - p0) > thres) && (abs(p3 - p0) > thres) && \
                (abs(p4 - p0) > thres) && (abs(p5 - p0) > thres) && (abs(p6 - p0) > thres) && \
                (abs(p7 - p0) > thres) && (abs(p8 - p0) > thres)) {
                if (mode == DpcMode::MEAN) {
                    // MEAN 模式: 上下左右 4 邻点均值(右移 2 位即除 4)
                    raw32_out[pixel_idx] = (p2 + p4 + p5 + p7) >> 2;
                } else {
                    //use gradient
                    // GRADIENT 模式: 计算 4 个方向的二阶梯度(拉普拉斯式)
                    // 梯度越小, 表示该方向越平滑(不含边缘), 用该方向邻点均值替代最安全
                    int dv = abs(2 * p0 - p2 - p7);     // 垂直方向梯度(上+下)
                    int dh = abs(2 * p0 - p4 - p5);     // 水平方向梯度(左+右)
                    int ddl = abs(2 * p0 - p1 - p8);    // 左对角梯度(左上+右下)
                    int ddr = abs(2 * p0 - p3 - p6);    // 右对角梯度(右上+左下)

                    int dvh_min = Min<int>(dv, dh);
                    int dlr_min = Min<int>(ddl, ddr);

                    int min_val = Min<int>(dvh_min, dlr_min);
                    //choose the fat border pixel and get mean of them
                    // 选择最小梯度方向的两邻点均值替代坏点(+1 用于四舍五入)
                    if (min_val == dv) {
                        raw32_out[pixel_idx] = (p2 + p7 + 1) >> 1;   // 垂直方向: 上+下
                    } else if (min_val == dh) {
                        raw32_out[pixel_idx] = (p4 + p5 + 1) >> 1;   // 水平方向: 左+右
                    } else if (min_val == ddl) {
                        raw32_out[pixel_idx] = (p1 + p8 + 1) >> 1;   // 左对角: 左上+右下
                    } else {
                        raw32_out[pixel_idx] = (p3 + p6 + 1)  >> 1;  // 右对角: 右上+左下
                    }
                }
            } else {
                // 非坏点: 直接拷贝
                raw32_out[pixel_idx] = static_cast<int>(raw32_in[pixel_idx]);
            }
        }
    }

    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, RAW -> RAW
void RegisterDpcMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::RAW;

    mod.name = MOD_NAME;
    mod.run_function = Dpc;

    RegisterIspModule(mod);
}