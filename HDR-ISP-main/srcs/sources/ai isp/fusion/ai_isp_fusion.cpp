/**
 * @file ai_isp_fusion.cpp
 * @brief AI ISP多分支数据融合实现
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
 * @brief 智能多分支融合模块
 * 融合主处理路径、AI分支和语义分支的输出
 */
class SmartMultiBranchFusion : public FusionModule {
public:
    SmartMultiBranchFusion() : FusionModule("smart_multibranch_fusion") {
        fusion_weights_ = {
            {"main_branch", 0.6f},      // 主分支权重
            {"ai_branch", 0.3f},        // AI分支权重
            {"semantic_branch", 0.1f}   // 语义分支权重
        };
        adaptive_fusion_ = true;        // 自适应融合
    }

    // 实现纯虚函数Process，提供默认的单输入处理
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override {
        // 对于融合模块，单输入时直接复制
        output = input;
        output.buffer_name = "fusion_output";

        AIISPResult result;
        result.success = true;
        result.output_buffer = output;
        return result;
    }

    AIISPResult FuseMultipleSources(
        const std::vector<AIDataBuffer>& inputs,
        const std::vector<NNFeatureData>& features,
        const SemanticData& semantic,
        AIDataBuffer& output) override {

        StartTimer();
        AIISPResult result;

        if (inputs.size() < 1) {
            result.success = false;
            result.error_message = "At least one input buffer required";
            return result;
        }

        // 获取主分支输出（第一个输入）
        const AIDataBuffer& main_output = inputs[0];

        // 设置输出缓冲区
        output.width = main_output.width;
        output.height = main_output.height;
        output.channels = main_output.channels;
        output.domain = main_output.domain;
        output.bit_depth = main_output.bit_depth;
        output.size = main_output.size;

        output.data = new uint8_t[output.size];
        output.buffer_name = "fusion_output";

        // 如果只有一个输入，直接复制
        if (inputs.size() == 1) {
            memcpy(output.data, main_output.data, main_output.size);
            result.success = true;
            result.output_buffer = output;
            result.processing_time_ms = EndTimer();
            return result;
        }

        // 多分支融合
        if (main_output.domain == DataDomain::RGB_DOMAIN && main_output.channels == 3) {
            if (adaptive_fusion_ && !features.empty()) {
                result = AdaptiveFusion(inputs, features, semantic, output);
            } else {
                result = WeightedFusion(inputs, output);
            }
        } else {
            // 非RGB数据，简单复制第一个输入
            memcpy(output.data, main_output.data, main_output.size);
        }

        result.processing_time_ms = EndTimer();
        return result;
    }

    /**
     * @brief 设置融合权重
     */
    void SetFusionWeights(const std::map<std::string, float>& weights) {
        fusion_weights_ = weights;
    }

    /**
     * @brief 启用/禁用自适应融合
     */
    void SetAdaptiveFusion(bool enable) {
        adaptive_fusion_ = enable;
    }

private:
    std::map<std::string, float> fusion_weights_;
    bool adaptive_fusion_;

    /**
     * @brief 加权融合
     */
    AIISPResult WeightedFusion(const std::vector<AIDataBuffer>& inputs, AIDataBuffer& output) {
        AIISPResult result;

        float* output_data = static_cast<float*>(output.data);
        size_t total_pixels = output.width * output.height * output.channels;

        // 初始化输出
        memset(output_data, 0, total_pixels * sizeof(float));

        // 加权融合所有输入
        float weight_sum = 0.0f;
        for (size_t i = 0; i < inputs.size(); ++i) {
            std::string branch_name = GetBranchName(i);
            float weight = GetBranchWeight(branch_name);
            weight_sum += weight;

            if (inputs[i].data != nullptr && inputs[i].size == output.size) {
                float* input_data = static_cast<float*>(inputs[i].data);
                for (size_t j = 0; j < total_pixels; ++j) {
                    output_data[j] += input_data[j] * weight;
                }
            }
        }

        // 归一化
        if (weight_sum > 0.0f) {
            for (size_t j = 0; j < total_pixels; ++j) {
                output_data[j] /= weight_sum;
                // 钳位到[0, 1]
                output_data[j] = std::max(0.0f, std::min(1.0f, output_data[j]));
            }
        } else {
            // 没有有效权重，复制第一个输入
            memcpy(output_data, inputs[0].data, inputs[0].size);
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }

    /**
     * @brief 自适应融合
     * 基于图像内容和语义信息进行智能融合
     */
    AIISPResult AdaptiveFusion(const std::vector<AIDataBuffer>& inputs,
                              const std::vector<NNFeatureData>& features,
                              const SemanticData& semantic,
                              AIDataBuffer& output) {
        AIISPResult result;

        float* output_data = static_cast<float*>(output.data);
        size_t total_pixels = output.width * output.height * 3; // RGB

        // 分析图像内容和语义信息
        auto content_analysis = AnalyzeImageContent(inputs[0], features, semantic);

        // 为每个像素计算自适应权重
        for (size_t i = 0; i < output.width * output.height; ++i) {
            size_t pixel_idx = i * 3;

            // 获取当前像素的语义类别
            uint8_t semantic_class = 0;
            if (!semantic.segmentation_map.empty() && i < semantic.segmentation_map.size()) {
                semantic_class = semantic.segmentation_map[i];
            }

            // 根据语义类别调整融合策略
            float main_weight = fusion_weights_["main_branch"];
            float ai_weight = fusion_weights_["ai_branch"];
            float semantic_weight = fusion_weights_["semantic_branch"];

            // 针对不同语义类别调整权重
            switch (semantic_class) {
                case 0: // 背景 - 使用主分支
                    main_weight = 0.8f;
                    ai_weight = 0.15f;
                    semantic_weight = 0.05f;
                    break;
                case 1: // 天空 - 增强AI处理
                    main_weight = 0.4f;
                    ai_weight = 0.5f;
                    semantic_weight = 0.1f;
                    break;
                case 2: // 人物 - 强化细节
                    main_weight = 0.3f;
                    ai_weight = 0.6f;
                    semantic_weight = 0.1f;
                    break;
                case 3: // 车辆 - 平衡处理
                    main_weight = 0.5f;
                    ai_weight = 0.4f;
                    semantic_weight = 0.1f;
                    break;
                default: // 其他 - 使用默认权重
                    break;
            }

            // 根据图像局部特征进一步调整
            float local_contrast = CalculateLocalContrast(inputs[0], i);
            if (local_contrast > 0.3f) { // 高对比度区域
                ai_weight *= 1.2f; // 增强AI处理
                main_weight *= 0.9f;
            }

            // 归一化权重
            float total_weight = main_weight + ai_weight + semantic_weight;
            main_weight /= total_weight;
            ai_weight /= total_weight;
            semantic_weight /= total_weight;

            // 融合像素值
            for (int c = 0; c < 3; ++c) {
                float fused_value = 0.0f;

                // 主分支
                if (inputs[0].data != nullptr) {
                    float* main_data = static_cast<float*>(inputs[0].data);
                    fused_value += main_data[pixel_idx + c] * main_weight;
                }

                // AI分支
                if (inputs.size() > 1 && inputs[1].data != nullptr) {
                    float* ai_data = static_cast<float*>(inputs[1].data);
                    fused_value += ai_data[pixel_idx + c] * ai_weight;
                }

                // 语义分支（如果有）
                if (inputs.size() > 2 && inputs[2].data != nullptr) {
                    float* semantic_data = static_cast<float*>(inputs[2].data);
                    fused_value += semantic_data[pixel_idx + c] * semantic_weight;
                }

                // 钳位
                output_data[pixel_idx + c] = std::max(0.0f, std::min(1.0f, fused_value));
            }
        }

        result.success = true;
        result.output_buffer = output;
        return result;
    }

    /**
     * @brief 分析图像内容
     */
    std::map<std::string, float> AnalyzeImageContent(const AIDataBuffer& input,
                                                     const std::vector<NNFeatureData>& features,
                                                     const SemanticData& semantic) {
        std::map<std::string, float> analysis;

        // 从特征数据中提取信息
        if (!features.empty()) {
            const auto& main_features = features[0];
            if (main_features.features.size() >= 4) {
                analysis["brightness"] = main_features.features[0];
                analysis["contrast"] = main_features.features[1];
                analysis["sharpness"] = main_features.features[2];
                analysis["noise_level"] = main_features.features[3];
            }
        }

        // 分析语义分布
        if (!semantic.segmentation_map.empty()) {
            std::vector<int> class_counts(10, 0);
            for (uint8_t class_id : semantic.segmentation_map) {
                if (class_id < 10) {
                    class_counts[class_id]++;
                }
            }

            float total = semantic.segmentation_map.size();
            analysis["background_ratio"] = class_counts[0] / total;
            analysis["sky_ratio"] = class_counts[1] / total;
            analysis["person_ratio"] = class_counts[2] / total;
            analysis["vehicle_ratio"] = class_counts[3] / total;
        }

        return analysis;
    }

    /**
     * @brief 计算局部对比度
     */
    float CalculateLocalContrast(const AIDataBuffer& input, size_t pixel_idx) {
        if (input.data == nullptr || input.channels != 3) {
            return 0.0f;
        }

        float* data = static_cast<float*>(input.data);
        size_t x = pixel_idx % input.width;
        size_t y = pixel_idx / input.width;

        // 简单的3x3邻域对比度计算
        std::vector<float> neighborhood_values;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;

                if (nx >= 0 && nx < static_cast<int>(input.width) &&
                    ny >= 0 && ny < static_cast<int>(input.height)) {

                    size_t neighbor_idx = (ny * input.width + nx) * 3;
                    float lum = 0.299f * data[neighbor_idx] +
                               0.587f * data[neighbor_idx + 1] +
                               0.114f * data[neighbor_idx + 2];
                    neighborhood_values.push_back(lum);
                }
            }
        }

        if (neighborhood_values.empty()) {
            return 0.0f;
        }

        // 计算标准差作为对比度
        float mean = 0.0f;
        for (float val : neighborhood_values) {
            mean += val;
        }
        mean /= neighborhood_values.size();

        float variance = 0.0f;
        for (float val : neighborhood_values) {
            variance += (val - mean) * (val - mean);
        }
        variance /= neighborhood_values.size();

        return std::sqrt(variance);
    }

    /**
     * @brief 获取分支名称
     */
    std::string GetBranchName(size_t index) {
        switch (index) {
            case 0: return "main_branch";
            case 1: return "ai_branch";
            case 2: return "semantic_branch";
            default: return "unknown_branch";
        }
    }

    /**
     * @brief 获取分支权重
     */
    float GetBranchWeight(const std::string& branch_name) {
        auto it = fusion_weights_.find(branch_name);
        if (it != fusion_weights_.end()) {
            return it->second;
        }
        return 0.0f;
    }
};

/**
 * @brief 语义感知融合模块
 * 基于语义分割结果的智能融合
 */
class SemanticAwareFusion : public FusionModule {
public:
    SemanticAwareFusion() : FusionModule("semantic_aware_fusion") {
        // 为不同语义类别设置处理强度
        semantic_processing_strength_ = {
            {0, 0.3f},  // 背景 - 低强度
            {1, 0.8f},  // 天空 - 高强度
            {2, 0.9f},  // 人物 - 最高强度
            {3, 0.7f},  // 车辆 - 中高强度
            {4, 0.5f}   // 建筑 - 中等强度
        };
    }

    // 实现纯虚函数Process，提供默认的单输入处理
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override {
        // 对于融合模块，单输入时直接复制
        output = input;
        output.buffer_name = "semantic_aware_fusion_output";

        AIISPResult result;
        result.success = true;
        result.output_buffer = output;
        return result;
    }

    AIISPResult FuseMultipleSources(
        const std::vector<AIDataBuffer>& inputs,
        const std::vector<NNFeatureData>& features,
        const SemanticData& semantic,
        AIDataBuffer& output) override {

        StartTimer();
        AIISPResult result;

        if (inputs.empty() || inputs[0].data == nullptr) {
            result.success = false;
            result.error_message = "Invalid inputs for semantic fusion";
            return result;
        }

        // 设置输出缓冲区
        const AIDataBuffer& main_input = inputs[0];
        output.width = main_input.width;
        output.height = main_input.height;
        output.channels = main_input.channels;
        output.domain = main_input.domain;
        output.bit_depth = main_input.bit_depth;
        output.size = main_input.size;

        output.data = new uint8_t[output.size];
        output.buffer_name = "semantic_aware_fusion_output";

        if (semantic.segmentation_map.empty()) {
            // 没有语义信息，简单复制
            memcpy(output.data, main_input.data, main_input.size);
            result.success = true;
            result.output_buffer = output;
            result.processing_time_ms = EndTimer();
            return result;
        }

        // 基于语义的智能融合
        float* output_data = static_cast<float*>(output.data);
        float* main_data = static_cast<float*>(main_input.data);
        float* ai_data = (inputs.size() > 1 && inputs[1].data != nullptr) ?
                        static_cast<float*>(inputs[1].data) : nullptr;

        for (size_t i = 0; i < main_input.width * main_input.height; ++i) {
            uint8_t semantic_class = semantic.segmentation_map[i];
            float processing_strength = GetSemanticStrength(semantic_class);

            size_t pixel_idx = i * 3;
            for (int c = 0; c < 3; ++c) {
                float main_val = main_data[pixel_idx + c];

                if (ai_data != nullptr) {
                    float ai_val = ai_data[pixel_idx + c];
                    // 基于语义强度的融合
                    output_data[pixel_idx + c] = main_val * (1.0f - processing_strength) +
                                                 ai_val * processing_strength;
                } else {
                    output_data[pixel_idx + c] = main_val;
                }

                // 钳位
                output_data[pixel_idx + c] = std::max(0.0f, std::min(1.0f, output_data[pixel_idx + c]));
            }
        }

        result.success = true;
        result.semantic_info = semantic;
        result.output_buffer = output;
        result.processing_time_ms = EndTimer();

        return result;
    }

    /**
     * @brief 设置语义处理强度
     */
    void SetSemanticStrength(uint8_t semantic_class, float strength) {
        semantic_processing_strength_[semantic_class] = strength;
    }

private:
    std::map<uint8_t, float> semantic_processing_strength_;

    float GetSemanticStrength(uint8_t semantic_class) {
        auto it = semantic_processing_strength_.find(semantic_class);
        if (it != semantic_processing_strength_.end()) {
            return it->second;
        }
        return 0.5f; // 默认中等强度
    }
};

} // namespace ai_isp