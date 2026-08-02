/**
 * @file frame.cpp
 * @brief 帧数据管理类的实现文件（中文说明）
 *
 * 本文件实现 Frame 类，负责管理 ISP pipeline 各阶段所需的图像内存缓冲区。
 * Frame 内部同时为 RAW / BGR / YUV 三个域（domain）分配输入(i)与输出(o)
 * 两个缓冲区，方便模块间做"交换内存"（SwapMem）实现零拷贝流水线。
 *
 * 关键设计：
 *   - 像素数量在宽高各方向额外预留 8 像素 padding，用于滤波器边界处理
 *     （避免越界访问，同时可存放 padding 后的边缘像素）
 *   - 每个域的 i/o 双缓冲，使前一模块输出与后一模块输入可通过指针交换完成
 */

#include "common/frame.h"

/**
 * @brief 为 Frame 的所有域分配内存
 *
 * @param width  图像宽度（不含 padding）
 * @param height 图像高度（不含 padding）
 * @return int   固定返回 0
 *
 * 内存布局说明：
 *   - pixel_nums = (width+8)*(height+8)，宽高各加 8 像素作为边界 padding，
 *     主要用于 LTM、CNS 等需要邻域窗口的算法，避免边界处出现越界读
 *   - RAW 域：raw_u8_i（按字节存原始打包数据，×2 因为 RAW16 每像素 2 字节）
 *             raw_u16_i / raw_u16_o（解包后的 16 位像素）
 *             raw_s32_i / raw_s32_o（高动态范围处理用的 32 位有符号整数）
 *   - BGR 域：bgr_s32_i / bgr_s32_o（×3 表示 RGB 三通道，用 32 位避免溢出）
 *             bgr_u8_o（最终输出给显示的 8 位 BGR）
 *   - YUV 域：分别用 float（中间处理，保证精度）与 uint8_t（最终输出）
 */
int Frame::FrameDateMalloc(int width, int height)
{
    // 计算 padding 后的像素总数，宽高各预留 8 像素边界
    auto pixel_nums = (width + 8) * (height + 8);
    // raw domain —— RAW 域缓冲区
    data.raw_u8_i = new uint8_t[pixel_nums * 2]; // max raw16，每个像素 2 字节
    data.raw_u16_i = new uint16_t[pixel_nums];   // max raw16，解包后 16 位像素
    data.raw_u16_o = new uint16_t[pixel_nums];   // max raw16，模块输出缓冲
    data.raw_s32_i = new int32_t[pixel_nums];    // 32 位有符号输入（HDR 高位宽处理）
    data.raw_s32_o = new int32_t[pixel_nums];    // 32 位有符号输出
    // bgr domain —— BGR 域缓冲区
    data.bgr_s32_i = new int32_t[pixel_nums * 3]; // 三通道，×3
    data.bgr_s32_o = new int32_t[pixel_nums * 3];
    // yuv domain —— YUV 域浮点缓冲（中间结果，保证计算精度）
    data.yuv_f32_i.y = new float[pixel_nums];
    data.yuv_f32_i.u = new float[pixel_nums];
    data.yuv_f32_i.v = new float[pixel_nums];
    data.yuv_f32_o.y = new float[pixel_nums];
    data.yuv_f32_o.u = new float[pixel_nums];
    data.yuv_f32_o.v = new float[pixel_nums];
    // yuv final out —— YUV 域 8 位输出缓冲（用于最终显示/编码）
    data.yuv_u8_i.y = new uint8_t[pixel_nums];
    data.yuv_u8_i.u = new uint8_t[pixel_nums];
    data.yuv_u8_i.v = new uint8_t[pixel_nums];
    data.yuv_u8_o.y = new uint8_t[pixel_nums];
    data.yuv_u8_o.u = new uint8_t[pixel_nums];
    data.yuv_u8_o.v = new uint8_t[pixel_nums];

    // 8 位 BGR 输出，供直接显示或保存为图片
    data.bgr_u8_o = new uint8_t[pixel_nums * 3];

    return 0;
}

/**
 * @brief 析构函数：释放 FrameDateMalloc 中所有 new[] 分配的内存
 *        使用 SAFE_FREE 宏（定义在 frame.h），保证 delete[] 后指针置空，
 *        避免悬垂指针与重复释放。
 */
Frame::~Frame()
{
    SAFE_FREE(data.raw_u8_i);
    SAFE_FREE(data.raw_u16_i);
    SAFE_FREE(data.raw_u16_o);
    SAFE_FREE(data.raw_s32_i);
    SAFE_FREE(data.raw_s32_o);
    SAFE_FREE(data.bgr_s32_i);
    SAFE_FREE(data.bgr_s32_o);
    SAFE_FREE(data.yuv_f32_i.y);
    SAFE_FREE(data.yuv_f32_i.u);
    SAFE_FREE(data.yuv_f32_i.v);
    SAFE_FREE(data.yuv_f32_o.y);
    SAFE_FREE(data.yuv_f32_o.u);
    SAFE_FREE(data.yuv_f32_o.v);
    SAFE_FREE(data.yuv_u8_i.y);
    SAFE_FREE(data.yuv_u8_i.u);
    SAFE_FREE(data.yuv_u8_i.v);
    SAFE_FREE(data.yuv_u8_o.y);
    SAFE_FREE(data.yuv_u8_o.u);
    SAFE_FREE(data.yuv_u8_o.v);
    SAFE_FREE(data.bgr_u8_o);
}

/**
 * @brief 构造函数：根据 ImageInfo 初始化一帧
 *
 * @param img_info 图像信息（宽、高、位宽、CFA 排列、数据类型等）
 *                 该引用会被拷贝保存到成员 info 中
 */
Frame::Frame(ImageInfo &img_info)
{
    info = img_info;
    FrameDateMalloc(info.width, info.height);
}

/**
 * @brief 将外部内存数据拷贝到 Frame 的 RAW 输入缓冲区
 *
 * @param src 源数据指针（通常是传感器输入或外部 buffer）
 * @param len 数据长度（字节）
 * @return int  0 成功；-1 长度超限；-2 目标缓冲未分配
 *
 * 长度校验：RAW16 每像素 2 字节，因此 len 不得超过 width*height*2
 */
int Frame::RawMemToFrame(void *src, int len)
{
    // 校验数据长度是否超出帧缓冲容量（按 RAW16 最大 2 字节/像素计算）
    if (len > (info.width * info.height * 2))
    {
        LOG(ERROR) << "Frame Size err";
        return -1;
    }
    if (data.raw_u8_i)
    {
        memcpy(data.raw_u8_i, src, len);
        return 0;
    }
    return -2;
}

/**
 * @brief 从磁盘文件读取 RAW 数据直接填充到 Frame 的 raw_u8_i 缓冲
 *
 * @param file 输入文件路径
 * @param size 需要读取的字节数
 * @return int  ReadFileToMem 的返回值（0 表示读取成功）
 */
int Frame::ReadFileToFrame(std::string file, int size)
{
    return (int)ReadFileToMem(file, data.raw_u8_i, size);
}