/**
 * @file ai_isp_types.h
 * @brief AI ISP数据类型定义
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#ifndef AI_ISP_TYPES_H
#define AI_ISP_TYPES_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstdint>

namespace ai_isp {

/**
 * @brief 数据域枚举
 */
enum class DataDomain {
    RAW_DOMAIN = 0,      // RAW域
    RGB_DOMAIN = 1,      // RGB域
    YUV_DOMAIN = 2,      // YUV域
    NN_DOMAIN = 3,       // 神经网络域
    SEMANTIC_DOMAIN = 4  // 语义分割域
};

/**
 * @brief 数据位深枚举
 */
enum class DataBitDepth {
    BIT8 = 8,
    BIT10 = 10,
    BIT12 = 12,
    BIT14 = 14,
    BIT16 = 16,
    BIT32 = 32
};

/**
 * @brief 处理分支枚举
 */
enum class ProcessBranch {
    MAIN_BRANCH = 0,        // 主处理分支
    AI_BRANCH = 1,          // AI处理分支
    SEMANTIC_BRANCH = 2,    // 语义分割分支
    FUSION_BRANCH = 3       // 融合分支
};

/**
 * @brief AI ISP模块基类配置
 */
struct AIModuleConfig {
    std::string module_name;           // 模块名称
    bool enable;                       // 是否启用
    ProcessBranch branch;              // 所属分支
    DataDomain input_domain;           // 输入数据域
    DataDomain output_domain;          // 输出数据域
    DataBitDepth input_bit_depth;      // 输入位深
    DataBitDepth output_bit_depth;     // 输出位深
    std::map<std::string, std::string> params; // 模块参数

    AIModuleConfig() : enable(true), branch(ProcessBranch::MAIN_BRANCH),
                      input_domain(DataDomain::RAW_DOMAIN),
                      output_domain(DataDomain::RGB_DOMAIN),
                      input_bit_depth(DataBitDepth::BIT16),
                      output_bit_depth(DataBitDepth::BIT8) {}
};

/**
 * @brief AI ISP数据缓冲区
 */
struct AIDataBuffer {
    void* data;                        // 数据指针
    size_t width;                      // 宽度
    size_t height;                     // 高度
    size_t channels;                   // 通道数
    DataDomain domain;                 // 数据域
    DataBitDepth bit_depth;            // 位深
    size_t size;                       // 数据大小(字节)
    std::string buffer_name;           // 缓冲区名称

    AIDataBuffer() : data(nullptr), width(0), height(0), channels(1),
                    domain(DataDomain::RAW_DOMAIN), bit_depth(DataBitDepth::BIT16),
                    size(0) {}

    ~AIDataBuffer() {
        // 注意：这里不释放内存，由外部管理
    }
};

/**
 * @brief 神经网络特征数据
 */
struct NNFeatureData {
    std::vector<float> features;       // 特征向量
    size_t feature_dim;                // 特征维度
    std::vector<size_t> spatial_dims;  // 空间维度 [H, W, C]
    ProcessBranch source_branch;       // 来源分支

    NNFeatureData() : feature_dim(0), source_branch(ProcessBranch::MAIN_BRANCH) {}
};

/**
 * @brief 语义分割数据
 */
struct SemanticData {
    std::vector<uint8_t> segmentation_map;  // 分割图
    size_t num_classes;                     // 类别数
    size_t width;                           // 宽度
    size_t height;                          // 高度
    std::vector<std::string> class_names;   // 类别名称

    SemanticData() : num_classes(0), width(0), height(0) {}
};

/**
 * @brief AI ISP处理结果
 */
struct AIISPResult {
    AIDataBuffer output_buffer;            // 输出缓冲区
    NNFeatureData nn_features;             // 神经网络特征
    SemanticData semantic_info;            // 语义信息
    bool success;                          // 处理是否成功
    std::string error_message;             // 错误信息
    double processing_time_ms;             // 处理时间(毫秒)

    AIISPResult() : success(false), processing_time_ms(0.0) {}
};

/**
 * @brief Pipeline分支配置
 */
struct PipelineBranchConfig {
    ProcessBranch branch_type;
    std::vector<std::string> module_names;
    bool enable_fusion;                    // 是否启用融合
    std::string fusion_strategy;           // 融合策略
    std::vector<ProcessBranch> input_branches; // 输入分支

    PipelineBranchConfig() : branch_type(ProcessBranch::MAIN_BRANCH),
                           enable_fusion(false), fusion_strategy("weighted") {}
};

/**
 * @brief 完整AI ISP Pipeline配置
 */
struct AIISPPipelineConfig {
    std::string pipeline_name;
    size_t input_width;
    size_t input_height;
    DataBitDepth input_bit_depth;
    std::string sensor_pattern;           // 传感器模式 "RGGB", "BGGR" etc.

    std::vector<PipelineBranchConfig> branches;
    std::map<std::string, AIModuleConfig> module_configs;

    bool enable_debug_output;
    std::string debug_output_path;

    AIISPPipelineConfig() : input_width(0), input_height(0),
                          input_bit_depth(DataBitDepth::BIT16),
                          sensor_pattern("RGGB"),
                          enable_debug_output(false) {}
};

} // namespace ai_isp

#endif // AI_ISP_TYPES_H