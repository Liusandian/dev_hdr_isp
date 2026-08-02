#ifndef IMAGE_SAVE_H
#define IMAGE_SAVE_H

#include <string>
#include <opencv2/opencv.hpp>
#include "common/frame.h"
#include "common/types.h"

/**
 * @brief 图像保存辅助类（中文说明）
 *
 * 该类提供用于保存ISP流水线中各个模块输入输出图像的功能。
 * 支持多种数据类型（uint8, uint16, int32等）和颜色域（RAW, BGR, YUV）。
 * 主要用于调试和可视化，方便查看各个处理阶段的效果。
 */
class ImageSaver {
public:
    /**
     * @brief 保存Frame对象中的图像数据
     *
     * @param frame 要保存的帧对象
     * @param filename 保存的文件名
     * @param data_type 要保存的数据类型（输入或输出）
     * @param domain 颜色域
     * @return int 0成功，-1失败
     */
    static int SaveFrame(const Frame* frame, const std::string& filename,
                        DataPtrTypes data_type, ColorDomains domain);

    /**
     * @brief 保存RAW域图像（单通道）
     *
     * @param data 图像数据指针
     * @param width 图像宽度
     * @param height 图像高度
     * @param data_type 数据类型
     * @param filename 保存文件名
     * @return int 0成功，-1失败
     */
    static int SaveRawImage(const void* data, int width, int height,
                           DataPtrTypes data_type, const std::string& filename);

    /**
     * @brief 保存BGR域图像（3通道）
     *
     * @param data 图像数据指针
     * @param width 图像宽度
     * @param height 图像高度
     * @param data_type 数据类型
     * @param filename 保存文件名
     * @return int 0成功，-1失败
     */
    static int SaveBgrImage(const void* data, int width, int height,
                           DataPtrTypes data_type, const std::string& filename);

    /**
     * @brief 保存YUV域图像（转换为BGR保存）
     *
     * @param y_mem Y分量内存结构
     * @param u_mem U分量内存结构
     * @param v_mem V分量内存结构
     * @param width 图像宽度
     * @param height 图像高度
     * @param data_type 数据类型
     * @param filename 保存文件名
     * @return int 0成功，-1失败
     */
    static int SaveYuvImage(const void* y, const void* u, const void* v,
                           int width, int height, DataPtrTypes data_type,
                           const std::string& filename);

    /**
     * @brief 创建输出目录（如果不存在）
     *
     * @param path 目录路径
     * @return int 0成功，-1失败
     */
    static int CreateOutputDirectory(const std::string& path);

private:
    /**
     * @brief 将数据转换为uint8类型并归一化
     *
     * @param src 源数据指针
     * @param dst 目标uint8数据指针
     * @param width 图像宽度
     * @param height 图像高度
     * @param channels 通道数
     * @param src_type 源数据类型
     * @param max_val 源数据的最大值（用于归一化）
     */
    static void ConvertToUint8(const void* src, uint8_t* dst, int width,
                              int height, int channels, DataPtrTypes src_type,
                              int max_val);
};

#endif // IMAGE_SAVE_H