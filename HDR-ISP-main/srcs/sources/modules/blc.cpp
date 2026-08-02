/**
 * @file blc.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief black level correct
 * @version 0.1
 * @date 2023-07-27
 * @copyright Copyright (c) 2023
 */
// =============================================================================
// 模块: blc (黑电平校正 / Black Level Correction)
// -----------------------------------------------------------------------------
// 功能: 从 RAW 像素中减去传感器黑电平偏置, 使无光照时的输出归零。
//
// 背景: CMOS 图像传感器即使在完全遮光条件下也会因暗电流、读出电路偏置等
//       原因输出一个非零的固定值(黑电平, Black Level)。该偏置会占用动态范围
//       的低端, 必须在 ISP 早期阶段减去, 否则会影响后续白平衡、伽马等模块
//       的准确性。黑电平通常由 sensor 遮光像素(OB pixels)统计得到。
//
// 算法: out = in - blc, 并限幅到 [0, max_val]。
// 数据流向: s32 -> s32 (色彩域保持 RAW)。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "blc"

// 黑电平校正主函数
static int Blc(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    int pwl_idx = 0;

    int32_t* raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);   // 解压后 RAW 输入
    int32_t* raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);  // 校正后 RAW 输出

    const DePwlPrms *pwl_prm = &(isp_prm->depwl_prm);

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);
            // 减去黑电平偏置, 消除传感器暗电流影响
            raw32_out[pixel_idx] = raw32_in[pixel_idx] - isp_prm->blc;
            // 限幅, 防止减法产生负值溢出
            ClipMinMax<int32_t>(raw32_out[pixel_idx], (int32_t)isp_prm->info.max_val, 0);
        }
    }

    //std::cout << "blc" << isp_prm->blc << "\n";

    // 交换缓冲, 使校正结果成为下一模块输入
    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o);

    return 0;
}

// 模块注册: s32 -> s32, RAW -> RAW
void RegisterBlcMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::RAW;

    mod.name = MOD_NAME;
    mod.run_function = Blc;

    RegisterIspModule(mod);
}