/**
 * @file raw_unpack.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: unpack (MIPI RAW 解包)
// -----------------------------------------------------------------------------
// 功能: 将传感器通过 MIPI CSI-2 接口传输的打包 RAW 数据解包为 16 位/像素的
//       线性 RAW 数据, 便于后续 ISP 模块处理。
//
// MIPI 联盟为减少传输带宽, 对 RAW 数据进行了紧凑打包:
//   - RAW10: 4 个 10 位像素打包到 5 字节(40 bit)中
//   - RAW12: 2 个 12 位像素打包到 3 字节(24 bit)中
//   - RAW16: 每个 16 位像素占 2 字节, 无额外打包
// 本模块将上述打包格式还原为统一的 uint16 数组(高位对齐, 低 6/4/0 位补 0)。
// 数据流向: u8 打包输入 -> u16 解包输出 (色彩域保持 RAW)。
// =============================================================================
#include "modules/modules.h"

#define MOD_NAME "unpack"

/**
 * @brief  mipi raw10 to raw16
 * [p1 9:2][p2 9:2][p3 9:2][p4 9:2][(p1 1:0)(p2 1:0)(p3 1:0)(p4 1:0)]
 * @param raw
 * @param unpack_raw16
 * @param width
 * @param height
 */
// -----------------------------------------------------------------------------
// RAW10 解包: 每 5 字节存放 4 个 10 位像素
//   字节0~3: 分别为 p1/p2/p3/p4 的高 8 位 (bit9..bit2)
//   字节4  : 低 2 位依次为 p1/p2/p3/p4 的 bit1..bit0
// 还原后像素值范围: 0~1023, 左移 2 位放入 uint16(低 2 位为原始低位)。
// -----------------------------------------------------------------------------
static void UnpackRaw10ToRaw16(uint8_t *raw, uint16_t *unpack_raw16, int width, int height)
{
    uint8_t *raw10_packed_in = raw;
    uint16_t *raw16_unpacked_out = unpack_raw16;
    int pixel_idx = 0;

    // p1 按字节递增 5(每 5 字节一组), p2 按像素递增 4(每组 4 像素)
    for (int p1 = 0, p2 = 0; p2 < width * height; p1 += 5, p2 += 4)
    {
        raw10_packed_in = raw + p1;
        raw16_unpacked_out = unpack_raw16 + p2;
        // 像素0: 高8位左移2 + 字节4的bit7..6
        raw16_unpacked_out[0] = ((uint16_t)raw10_packed_in[0] << 2) | (((uint16_t)raw10_packed_in[4] >> 6) & 0x03);
        // 像素1: 高8位左移2 + 字节4的bit5..4
        raw16_unpacked_out[1] = ((uint16_t)raw10_packed_in[1] << 2) | (((uint16_t)raw10_packed_in[4] >> 4) & 0x03);
        // 像素2: 高8位左移2 + 字节4的bit3..2
        raw16_unpacked_out[2] = ((uint16_t)raw10_packed_in[2] << 2) | (((uint16_t)raw10_packed_in[4] >> 2) & 0x03);
        // 像素3: 高8位左移2 + 字节4的bit1..0
        raw16_unpacked_out[3] = ((uint16_t)raw10_packed_in[3] << 2) | (((uint16_t)raw10_packed_in[4] >> 0) & 0x03);
    }
}
/**
 * @brief  mipi raw12 to raw16
 * [p1 11:4][p2 11:4][(p1 3:0)(p1 3:0)]
 * @param raw
 * @param unpack_raw16
 * @param width
 * @param height
 */
// -----------------------------------------------------------------------------
// RAW12 解包: 每 3 字节存放 2 个 12 位像素
//   字节0: p1 高 8 位 (bit11..bit4)
//   字节1: p2 高 8 位 (bit11..bit4)
//   字节2: 高 4 位为 p1 低 4 位, 低 4 位为 p2 低 4 位
// 还原后像素值范围: 0~4095, 左移 4 位放入 uint16。
// -----------------------------------------------------------------------------
static void UnpackRaw12ToRaw16(uint8_t *raw, uint16_t *unpack_raw16, int width, int height)
{
    uint8_t *raw12_packed_in = raw;
    uint16_t *raw16_unpacked_out = unpack_raw16;

    // p1 按字节递增 3, p2 按像素递增 2
    for (int p1 = 0, p2 = 0; p2 < width * height; p1 += 3, p2 += 2)
    {
        raw12_packed_in = raw + p1;
        raw16_unpacked_out = unpack_raw16 + p2;
        // 像素0: 高8位左移4 + 字节2高4位
        raw16_unpacked_out[0] = (raw12_packed_in[0] << 4) | ((raw12_packed_in[3] >> 4) & 0x0f);
        // 像素1: 高8位左移4 + 字节2低4位
        raw16_unpacked_out[1] = (raw12_packed_in[1] << 4) | ((raw12_packed_in[3] >> 0) & 0x0f);
    }
}

/**
 * @brief  mipi raw16 to raw16
 * [p1 15: 8][p1 7 : 0]
 * @param raw
 * @param unpack_raw16
 * @param width
 * @param height
 */
// RAW16 无需解包, 直接按字节拷贝(每像素 2 字节, 小端序)
static void UnpackRaw16ToRaw16(uint8_t *raw, uint16_t *unpack_raw16, int width, int height)
{
    uint8_t *raw16_packed_in = raw;
    uint16_t *raw16_unpacked_out = unpack_raw16;

    memcpy(raw16_unpacked_out, raw16_packed_in, width * height * 2);
}

// 主处理函数: 根据帧的 RAW 数据类型选择对应解包算法
static int MipiDataUnpack(Frame *frame, const IspPrms *prms)
{
    if ((frame == nullptr) || (prms == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }

    switch (frame->info.dt)
    {
    case RawDataTypes::RAW10:
        UnpackRaw10ToRaw16((uint8_t *)frame->data.raw_u8_i, (uint16_t *)frame->data.raw_u16_o, frame->info.width, frame->info.height);
        break;
    case RawDataTypes::RAW12:
        UnpackRaw12ToRaw16((uint8_t *)frame->data.raw_u8_i, (uint16_t *)frame->data.raw_u16_o, frame->info.width, frame->info.height);
        break;
    case RawDataTypes::RAW16:
        UnpackRaw16ToRaw16((uint8_t *)frame->data.raw_u8_i, (uint16_t *)frame->data.raw_u16_o, frame->info.width, frame->info.height);
        break;
    default:
        break;
    }

    // 交换输入/输出缓冲, 使解包结果成为下一模块的输入
    SwapMem<void>(frame->data.raw_u16_o, frame->data.raw_u16_i);

    return 0;
}

// 模块注册: 声明数据类型(u8->u16)与色彩域(RAW->RAW)
void RegisterUnpackMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_UINT8;
    mod.out_type = DataPtrTypes::TYPE_UINT16;

    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::RAW;

    mod.name = MOD_NAME;

    mod.run_function = MipiDataUnpack;

    RegisterIspModule(mod);
}