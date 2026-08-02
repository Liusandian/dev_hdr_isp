#include "common/image_save.h"
#include <iostream>
#include <filesystem>
#include "easylogging++.h"

namespace fs = std::filesystem;

int ImageSaver::CreateOutputDirectory(const std::string& path)
{
    try {
        if (!fs::exists(path)) {
            if (fs::create_directories(path)) {
                LOG(INFO) << "Created output directory: " << path;
                return 0;
            } else {
                LOG(ERROR) << "Failed to create directory: " << path;
                return -1;
            }
        }
        return 0;
    } catch (const fs::filesystem_error& e) {
        LOG(ERROR) << "Filesystem error: " << e.what();
        return -1;
    }
}

void ImageSaver::ConvertToUint8(const void* src, uint8_t* dst, int width,
                               int height, int channels, DataPtrTypes src_type,
                               int max_val)
{
    int total_pixels = width * height * channels;

    switch (src_type) {
        case DataPtrTypes::TYPE_UINT8: {
            const uint8_t* src_ptr = static_cast<const uint8_t*>(src);
            for (int i = 0; i < total_pixels; ++i) {
                dst[i] = src_ptr[i];
            }
            break;
        }
        case DataPtrTypes::TYPE_UINT16: {
            const uint16_t* src_ptr = static_cast<const uint16_t*>(src);
            for (int i = 0; i < total_pixels; ++i) {
                dst[i] = static_cast<uint8_t>((src_ptr[i] * 255.0f) / max_val);
            }
            break;
        }
        case DataPtrTypes::TYPE_INT16: {
            const int16_t* src_ptr = static_cast<const int16_t*>(src);
            for (int i = 0; i < total_pixels; ++i) {
                int val = src_ptr[i];
                val = (val < 0) ? 0 : val;
                val = (val > max_val) ? max_val : val;
                dst[i] = static_cast<uint8_t>((val * 255.0f) / max_val);
            }
            break;
        }
        case DataPtrTypes::TYPE_INT32: {
            const int32_t* src_ptr = static_cast<const int32_t*>(src);
            for (int i = 0; i < total_pixels; ++i) {
                int val = src_ptr[i];
                val = (val < 0) ? 0 : val;
                val = (val > max_val) ? max_val : val;
                dst[i] = static_cast<uint8_t>((val * 255.0f) / max_val);
            }
            break;
        }
        case DataPtrTypes::TYPE_FLOAT32: {
            const float* src_ptr = static_cast<const float*>(src);
            for (int i = 0; i < total_pixels; ++i) {
                float val = src_ptr[i];
                val = (val < 0.0f) ? 0.0f : val;
                val = (val > 1.0f) ? 1.0f : val;
                dst[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        default:
            LOG(ERROR) << "Unsupported data type for conversion";
            break;
    }
}

int ImageSaver::SaveRawImage(const void* data, int width, int height,
                            DataPtrTypes data_type, const std::string& filename)
{
    if (!data || width <= 0 || height <= 0) {
        LOG(ERROR) << "Invalid parameters for RAW image saving";
        return -1;
    }

    // 创建单通道灰度图
    cv::Mat img(height, width, CV_8UC1);
    int max_val = 255; // 默认最大值

    // 根据数据类型设置合适的最大值
    switch (data_type) {
        case DataPtrTypes::TYPE_UINT16:
            max_val = 65535;
            break;
        case DataPtrTypes::TYPE_INT16:
            max_val = 32767;
            break;
        case DataPtrTypes::TYPE_INT32:
            max_val = 255; // 假设已经是0-255范围
            break;
        default:
            break;
    }

    ConvertToUint8(data, img.data, width, height, 1, data_type, max_val);

    // 应用伪彩色以更好显示RAW数据
    cv::Mat color_img;
    cv::applyColorMap(img, color_img, cv::COLORMAP_JET);

    if (cv::imwrite(filename, color_img)) {
        LOG(INFO) << "Saved RAW image to: " << filename;
        return 0;
    } else {
        LOG(ERROR) << "Failed to save RAW image to: " << filename;
        return -1;
    }
}

int ImageSaver::SaveBgrImage(const void* data, int width, int height,
                            DataPtrTypes data_type, const std::string& filename)
{
    if (!data || width <= 0 || height <= 0) {
        LOG(ERROR) << "Invalid parameters for BGR image saving";
        return -1;
    }

    // 创建3通道BGR图像
    cv::Mat img(height, width, CV_8UC3);
    int max_val = 255;

    // 根据数据类型设置合适的最大值
    switch (data_type) {
        case DataPtrTypes::TYPE_UINT16:
            max_val = 65535;
            break;
        case DataPtrTypes::TYPE_INT16:
            max_val = 32767;
            break;
        case DataPtrTypes::TYPE_INT32:
            max_val = 255;
            break;
        default:
            break;
    }

    ConvertToUint8(data, img.data, width, height, 3, data_type, max_val);

    if (cv::imwrite(filename, img)) {
        LOG(INFO) << "Saved BGR image to: " << filename;
        return 0;
    } else {
        LOG(ERROR) << "Failed to save BGR image to: " << filename;
        return -1;
    }
}

int ImageSaver::SaveYuvImage(const void* y, const void* u, const void* v,
                            int width, int height, DataPtrTypes data_type,
                            const std::string& filename)
{
    if (!y || !u || !v || width <= 0 || height <= 0) {
        LOG(ERROR) << "Invalid parameters for YUV image saving";
        return -1;
    }

    // 创建临时缓冲区
    int total_pixels = width * height;
    std::vector<float> y_buffer(total_pixels);
    std::vector<float> u_buffer(total_pixels);
    std::vector<float> v_buffer(total_pixels);

    // 根据数据类型转换YUV数据
    switch (data_type) {
        case DataPtrTypes::TYPE_FLOAT32: {
            const float* y_ptr = static_cast<const float*>(y);
            const float* u_ptr = static_cast<const float*>(u);
            const float* v_ptr = static_cast<const float*>(v);
            std::copy(y_ptr, y_ptr + total_pixels, y_buffer.begin());
            std::copy(u_ptr, u_ptr + total_pixels, u_buffer.begin());
            std::copy(v_ptr, v_ptr + total_pixels, v_buffer.begin());
            break;
        }
        case DataPtrTypes::TYPE_UINT8: {
            const uint8_t* y_ptr = static_cast<const uint8_t*>(y);
            const uint8_t* u_ptr = static_cast<const uint8_t*>(u);
            const uint8_t* v_ptr = static_cast<const uint8_t*>(v);
            for (int i = 0; i < total_pixels; ++i) {
                y_buffer[i] = y_ptr[i] / 255.0f;
                u_buffer[i] = u_ptr[i] / 255.0f;
                v_buffer[i] = v_ptr[i] / 255.0f;
            }
            break;
        }
        default:
            LOG(ERROR) << "Unsupported YUV data type";
            return -1;
    }

    // 创建BGR图像
    cv::Mat img(height, width, CV_8UC3);

    // YUV转BGR
    for (int i = 0; i < total_pixels; ++i) {
        float y_val = y_buffer[i];
        float u_val = u_buffer[i];
        float v_val = v_buffer[i];

        // YUV转RGB公式
        float r_val = y_val + 1.402f * (v_val - 0.5f);
        float g_val = y_val - 0.344136f * (u_val - 0.5f) - 0.714136f * (v_val - 0.5f);
        float b_val = y_val + 1.772f * (u_val - 0.5f);

        // 钳制到[0,1]范围并转换为8位
        img.data[i * 3 + 0] = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, b_val)) * 255.0f);
        img.data[i * 3 + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, g_val)) * 255.0f);
        img.data[i * 3 + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, r_val)) * 255.0f);
    }

    if (cv::imwrite(filename, img)) {
        LOG(INFO) << "Saved YUV image to: " << filename;
        return 0;
    } else {
        LOG(ERROR) << "Failed to save YUV image to: " << filename;
        return -1;
    }
}

int ImageSaver::SaveFrame(const Frame* frame, const std::string& filename,
                         DataPtrTypes data_type, ColorDomains domain)
{
    if (!frame) {
        LOG(ERROR) << "Null frame pointer";
        return -1;
    }

    const ImageInfo& info = frame->info;
    const ImageMem& mem = frame->data;

    void* data_ptr = nullptr;

    // 根据数据类型和颜色域选择合适的数据指针
    if (domain == ColorDomains::RAW) {
        if (data_type == DataPtrTypes::TYPE_UINT8) {
            data_ptr = mem.raw_u8_i;
        } else if (data_type == DataPtrTypes::TYPE_UINT16) {
            // 对于uint16，优先使用输出数据
            data_ptr = mem.raw_u16_o ? mem.raw_u16_o : mem.raw_u16_i;
        } else if (data_type == DataPtrTypes::TYPE_INT32) {
            data_ptr = mem.raw_s32_i;
        }
    } else if (domain == ColorDomains::BGR) {
        if (data_type == DataPtrTypes::TYPE_INT32) {
            data_ptr = mem.bgr_s32_o ? mem.bgr_s32_o : mem.bgr_s32_i;
        } else if (data_type == DataPtrTypes::TYPE_UINT8) {
            data_ptr = mem.bgr_u8_o;
        }
    } else if (domain == ColorDomains::YUV) {
        if (data_type == DataPtrTypes::TYPE_FLOAT32) {
            // 保存YUV图像需要三个分量
            return SaveYuvImage(mem.yuv_f32_o.y ? mem.yuv_f32_o.y : mem.yuv_f32_i.y,
                              mem.yuv_f32_o.u ? mem.yuv_f32_o.u : mem.yuv_f32_i.u,
                              mem.yuv_f32_o.v ? mem.yuv_f32_o.v : mem.yuv_f32_i.v,
                              info.width, info.height, data_type, filename);
        } else if (data_type == DataPtrTypes::TYPE_UINT8) {
            return SaveYuvImage(mem.yuv_u8_o.y ? mem.yuv_u8_o.y : mem.yuv_u8_i.y,
                              mem.yuv_u8_o.u ? mem.yuv_u8_o.u : mem.yuv_u8_i.u,
                              mem.yuv_u8_o.v ? mem.yuv_u8_o.v : mem.yuv_u8_i.v,
                              info.width, info.height, data_type, filename);
        }
    }

    if (!data_ptr) {
        LOG(WARNING) << "No valid data pointer found for the given type and domain";
        return -1;
    }

    // 根据颜色域调用相应的保存函数
    switch (domain) {
        case ColorDomains::RAW:
            return SaveRawImage(data_ptr, info.width, info.height, data_type, filename);
        case ColorDomains::BGR:
            return SaveBgrImage(data_ptr, info.width, info.height, data_type, filename);
        case ColorDomains::YUV:
            // YUV已经在前面处理过了
            break;
        default:
            LOG(ERROR) << "Unsupported color domain: " << static_cast<int>(domain);
            return -1;
    }

    return -1;
}