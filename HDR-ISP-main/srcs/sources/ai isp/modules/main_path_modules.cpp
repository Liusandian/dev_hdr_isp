/**
 * @file main_path_modules.cpp
 * @brief 主处理路径模块实现
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#include "ai_isp_module.h"
#include "ai_isp_types.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ai_isp {

/**
 * @brief RAW前端处理模块 (rawfe)
 * 处理RAW数据的基本前端操作
 */
class RawFrontEnd : public TraditionalISPModule {
public:
    RawFrontEnd() : TraditionalISPModule("rawfe") {}

    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override {
        AIISPResult result;

        // 输入验证
        if (input.data == nullptr || input.width == 0 || input.height == 0) {
            result.success = false;
            result.error_message = "Invalid input data";
            return result;
        }

        // 设置输出缓冲区属性
        output.width = input.width;
        output.height = input.height;
        output.channels = 1; // RAW数据是单通道
        output.domain = DataDomain::RAW_DOMAIN;
        output.bit_depth = input.bit_depth;
        output.size = input.width * input.height * (static_cast<int>(input.bit_depth) / 8);

        // 分配输出缓冲区
        output.data = new uint8_t[output.size];
        output.buffer_name = "rawfe_output";

        // 简单的RAW前端处理：复制数据并应用黑电平补偿
        uint16_t* input_data = static_cast<uint16_t*>(input.data);
        uint16_t* output_data = static_cast<uint16_t*>(output.data);

        uint16_t black_level = 64; // 假设黑电平为64
        size_t total_pixels = input.width * input.height;

        for (size_t i = 0; i < total_pixels; ++i) {
            // 黑电平补偿
            int32_t temp = static_cast<int32_t>(input_data[i]) - black_level;
            temp = std::max(0, std::min(65535, temp)); // 钳位到[0, 65535]
            output_data[i] = static_cast<uint16_t>(temp);
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }
};

/**
 * @brief 神经网络特征提取模块 (nnxf)
 * 从RAW数据中提取神经网络特征
 */
class NNFeatureExtractor : public NNModule {
public:
    NNFeatureExtractor() : NNModule("nnxf") {}

    bool LoadModel(const std::string& model_path) override {
        model_path_ = model_path;
        model_loaded_ = true;
        return true;
    }

    NNFeatureData Inference(const AIDataBuffer& input) override {
        NNFeatureData features;

        if (!model_loaded_ || input.data == nullptr) {
            return features;
        }

        // 仿真：从RAW数据中提取简单特征
        uint16_t* raw_data = static_cast<uint16_t*>(input.data);
        size_t total_pixels = input.width * input.height;

        // 计算基本统计特征
        double sum = 0.0, sum_sq = 0.0;
        uint16_t min_val = 65535, max_val = 0;

        for (size_t i = 0; i < total_pixels; ++i) {
            uint16_t val = raw_data[i];
            sum += val;
            sum_sq += val * val;
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }

        double mean = sum / total_pixels;
        double variance = (sum_sq / total_pixels) - (mean * mean);
        double std_dev = std::sqrt(std::max(0.0, variance));

        // 构建特征向量
        features.features = {
            static_cast<float>(mean / 65535.0),      // 归一化均值
            static_cast<float>(std_dev / 65535.0),   // 归一化标准差
            static_cast<float>(min_val / 65535.0),   // 归一化最小值
            static_cast<float>(max_val / 65535.0),   // 归一化最大值
            static_cast<float>((max_val - min_val) / 65535.0) // 归一化动态范围
        };

        features.feature_dim = features.features.size();
        features.spatial_dims = {1, 1, features.feature_dim};
        features.source_branch = ProcessBranch::MAIN_BRANCH;

        return features;
    }

    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override {
        // nnxf模块主要输出特征，不修改主数据流
        output = input; // 直接传递输入
        output.buffer_name = "nnxf_output";

        AIISPResult result;
        result.success = true;
        result.output_buffer = output;
        result.nn_features = Inference(input);
        return result;
    }
};

/**
 * @brief RGB镜头阴影校正模块 (rgblsc)
 * 修正镜头阴影不均匀
 */
class RGBLSC : public TraditionalISPModule {
public:
    RGBLSC() : TraditionalISPModule("rgblsc") {
        // 初始化LSC增益表（简化版本）
        InitializeLSCGainTable();
    }

    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override {
        AIISPResult result;

        if (input.data == nullptr || input.channels != 3) {
            result.success = false;
            result.error_message = "Invalid RGB input for LSC";
            return result;
        }

        // 设置输出缓冲区
        output.width = input.width;
        output.height = input.height;
        output.channels = 3;
        output.domain = DataDomain::RGB_DOMAIN;
        output.bit_depth = input.bit_depth;
        output.size = input.width * input.height * 3 * (static_cast<int>(input.bit_depth) / 8);

        output.data = new uint8_t[output.size];
        output.buffer_name = "rgblsc_output";

        // 应用LSC校正
        float* input_data = static_cast<float*>(input.data);
        float* output_data = static_cast<float*>(output.data);

        for (size_t y = 0; y < input.height; ++y) {
            for (size_t x = 0; x < input.width; ++x) {
                // 计算LSC增益（简化版：基于距离图像中心的距离）
                float center_x = input.width / 2.0f;
                float center_y = input.height / 2.0f;
                float dist = std::sqrt((x - center_x) * (x - center_x) +
                                     (y - center_y) * (y - center_y));
                float max_dist = std::sqrt(center_x * center_x + center_y * center_y);
                float gain = 1.0f + 0.3f * (dist / max_dist); // 边缘增益最高1.3倍

                for (int c = 0; c < 3; ++c) {
                    size_t idx = (y * input.width + x) * 3 + c;
                    output_data[idx] = input_data[idx] * gain;
                    // 钳位到[0, 1.0]
                    output_data[idx] = std::max(0.0f, std::min(1.0f, output_data[idx]));
                }
            }
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }

private:
    void InitializeLSCGainTable() {
        // 实际实现中，这里会从标定数据加载LSC增益表
        // 这里只是简化版本
    }
};

/**
 * @brief RGB动态范围压缩模块 (rgbdrc)
 * 压缩高动态范围图像
 */
class RGBDRC : public TraditionalISPModule {
public:
    RGBDRC() : TraditionalISPModule("rgbdrc") {}

    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override {
        AIISPResult result;

        if (input.data == nullptr || input.channels != 3) {
            result.success = false;
            result.error_message = "Invalid RGB input for DRC";
            return result;
        }

        // 设置输出缓冲区
        output.width = input.width;
        output.height = input.height;
        output.channels = 3;
        output.domain = DataDomain::RGB_DOMAIN;
        output.bit_depth = input.bit_depth;
        output.size = input.size;

        output.data = new uint8_t[output.size];
        output.buffer_name = "rgbdrc_output";

        // 应用自适应动态范围压缩
        float* input_data = static_cast<float*>(input.data);
        float* output_data = static_cast<float*>(output.data);

        // 计算图像亮度统计
        std::vector<float> luminance(input.width * input.height);
        for (size_t i = 0; i < luminance.size(); ++i) {
            size_t idx = i * 3;
            luminance[i] = 0.299f * input_data[idx] + 0.587f * input_data[idx + 1] +
                          0.114f * input_data[idx + 2];
        }

        // 计算直方图
        const int hist_bins = 256;
        std::vector<int> histogram(hist_bins, 0);
        for (float lum : luminance) {
            int bin = std::min(hist_bins - 1, static_cast<int>(lum * hist_bins));
            histogram[bin]++;
        }

        // 找到1%和99%百分点
        int total_pixels = luminance.size();
        int low_threshold = total_pixels / 100;
        int high_threshold = total_pixels * 99 / 100;

        float low_lum = 0.0f, high_lum = 1.0f;
        int cumulative = 0;
        for (int i = 0; i < hist_bins; ++i) {
            cumulative += histogram[i];
            if (cumulative >= low_threshold && low_lum == 0.0f) {
                low_lum = static_cast<float>(i) / hist_bins;
            }
            if (cumulative >= high_threshold) {
                high_lum = static_cast<float>(i) / hist_bins;
                break;
            }
        }

        // 应用动态范围压缩
        float range = std::max(0.01f, high_lum - low_lum);
        for (size_t i = 0; i < luminance.size(); ++i) {
            size_t idx = i * 3;
            float compressed_lum = (luminance[i] - low_lum) / range;
            compressed_lum = std::max(0.0f, std::min(1.0f, compressed_lum));

            // 保持颜色比例，应用压缩
            float scale = (luminance[i] > 0.01f) ? (compressed_lum / luminance[i]) : 1.0f;
            for (int c = 0; c < 3; ++c) {
                output_data[idx + c] = input_data[idx + c] * scale;
                output_data[idx + c] = std::max(0.0f, std::min(1.0f, output_data[idx + c]));
            }
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }
};

/**
 * @brief 颜色校正矩阵模块 (ccm)
 * 应用颜色校正矩阵
 */
class ColorCorrectionMatrix : public TraditionalISPModule {
public:
    ColorCorrectionMatrix() : TraditionalISPModule("ccm") {
        // 初始化标准CCM矩阵（sRGB到XYZ的近似逆变换）
        ccm_matrix_ = {
            1.67f, -0.42f, -0.26f,  // R
            -0.39f, 1.21f, -0.16f,  // G
            -0.08f, -0.21f, 1.30f   // B
        };
    }

    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override {
        AIISPResult result;

        if (input.data == nullptr || input.channels != 3) {
            result.success = false;
            result.error_message = "Invalid RGB input for CCM";
            return result;
        }

        // 设置输出缓冲区
        output = input;
        output.data = new uint8_t[input.size];
        output.buffer_name = "ccm_output";

        // 应用颜色校正矩阵
        float* input_data = static_cast<float*>(input.data);
        float* output_data = static_cast<float*>(output.data);

        for (size_t i = 0; i < input.width * input.height; ++i) {
            size_t idx = i * 3;
            float r = input_data[idx];
            float g = input_data[idx + 1];
            float b = input_data[idx + 2];

            // 矩阵乘法
            output_data[idx] = ccm_matrix_[0] * r + ccm_matrix_[1] * g + ccm_matrix_[2] * b;
            output_data[idx + 1] = ccm_matrix_[3] * r + ccm_matrix_[4] * g + ccm_matrix_[5] * b;
            output_data[idx + 2] = ccm_matrix_[6] * r + ccm_matrix_[7] * g + ccm_matrix_[8] * b;

            // 钳位到[0, 1]
            for (int c = 0; c < 3; ++c) {
                output_data[idx + c] = std::max(0.0f, std::min(1.0f, output_data[idx + c]));
            }
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }

private:
    std::vector<float> ccm_matrix_;
};

/**
 * @brief 伽马校正模块 (gamma)
 * 应用伽马校正曲线
 */
class GammaCorrection : public TraditionalISPModule {
public:
    GammaCorrection() : TraditionalISPModule("gamma") {
        gamma_value_ = 2.2f; // 标准伽马值
        InitializeGammaTable();
    }

    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override {
        AIISPResult result;

        if (input.data == nullptr) {
            result.success = false;
            result.error_message = "Invalid input for gamma correction";
            return result;
        }

        // 设置输出缓冲区
        output = input;
        output.data = new uint8_t[input.size];
        output.buffer_name = "gamma_output";

        // 应用伽马校正
        if (input.channels == 3) { // RGB数据
            float* input_data = static_cast<float*>(input.data);
            float* output_data = static_cast<float*>(output.data);

            for (size_t i = 0; i < input.width * input.height * 3; ++i) {
                output_data[i] = gamma_table_[static_cast<int>(input_data[i] * 255.0)];
            }
        } else { // 单通道数据
            float* input_data = static_cast<float*>(input.data);
            float* output_data = static_cast<float*>(output.data);

            for (size_t i = 0; i < input.width * input.height; ++i) {
                output_data[i] = gamma_table_[static_cast<int>(input_data[i] * 255.0)];
            }
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }

    void SetGamma(float gamma) {
        gamma_value_ = gamma;
        InitializeGammaTable();
    }

private:
    float gamma_value_;
    std::vector<float> gamma_table_;

    void InitializeGammaTable() {
        gamma_table_.resize(256);
        for (int i = 0; i < 256; ++i) {
            float normalized = static_cast<float>(i) / 255.0f;
            gamma_table_[i] = std::pow(normalized, 1.0f / gamma_value_);
        }
    }
};

} // namespace ai_isp