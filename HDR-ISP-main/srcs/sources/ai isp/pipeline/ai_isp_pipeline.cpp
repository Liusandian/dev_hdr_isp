/**
 * @file ai_isp_pipeline.cpp
 * @brief AI ISP Pipeline管理系统
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#include "ai_isp_module.h"
#include "ai_isp_types.h"
#include "ai_isp_fusion.h"
#include "ai_isp_pipeline.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <fstream>

namespace ai_isp {

// BranchProcessor实现
BranchProcessor::BranchProcessor(const std::string& name, ProcessBranch branch_type)
    : branch_name_(name), branch_type_(branch_type) {}

void BranchProcessor::AddModule(AIISPModulePtr module) {
    modules_.push_back(module);
}

AIISPResult BranchProcessor::Process(const AIDataBuffer& input,
                       const NNFeatureData& features,
                       const SemanticData& semantic) {

    if (modules_.empty()) {
        AIISPResult result;
        result.success = false;
        result.error_message = "No modules in branch";
        return result;
    }

    AIDataBuffer current_input = input;
    AIDataBuffer current_output;
    AIISPResult final_result;
    NNFeatureData current_features = features;

    // 依次处理每个模块
    for (auto& module : modules_) {
        if (!module->IsEnabled()) {
            continue; // 跳过禁用的模块
        }

        // 根据模块类型选择处理方式
        if (auto nn_module = std::dynamic_pointer_cast<NNModule>(module)) {
            // 神经网络模块
            NNFeatureData new_features = nn_module->Inference(current_input);
            if (!new_features.features.empty()) {
                current_features = new_features;
            }

            current_output = current_input; // NN模块通常不修改主数据流
            current_output.buffer_name = module->GetModuleName() + "_output";

        } else if (auto fusion_module = std::dynamic_pointer_cast<FusionModule>(module)) {
            // 融合模块需要特殊处理
            std::vector<AIDataBuffer> inputs = {current_input};
            AIISPResult fusion_result = fusion_module->FuseMultipleSources(
                inputs, {current_features}, semantic, current_output);

            if (!fusion_result.success) {
                final_result = fusion_result;
                return final_result;
            }

            if (!fusion_result.nn_features.features.empty()) {
                current_features = fusion_result.nn_features;
            }

        } else {
            // 传统处理模块
            AIISPResult module_result = module->Process(current_input, current_output);
            if (!module_result.success) {
                final_result = module_result;
                return final_result;
            }

            // 累积处理时间
            final_result.processing_time_ms += module_result.processing_time_ms;

            // 更新特征数据
            if (!module_result.nn_features.features.empty()) {
                current_features = module_result.nn_features;
            }
        }

        // 准备下一个模块的输入
        if (current_output.data != nullptr) {
            // 释放当前输入内存（如果不是原始输入）
            if (current_input.data != input.data && current_input.data != nullptr) {
                delete[] static_cast<uint8_t*>(current_input.data);
            }
            current_input = current_output;
        }
    }

    final_result.success = true;
    final_result.output_buffer = current_input;
    final_result.nn_features = current_features;

    return final_result;
}

std::string BranchProcessor::GetBranchName() const {
    return branch_name_;
}

ProcessBranch BranchProcessor::GetBranchType() const {
    return branch_type_;
}

size_t BranchProcessor::GetModuleCount() const {
    return modules_.size();
}

// AIISPPipeline实现
AIISPPipeline::AIISPPipeline() {
    pipeline_config_ = std::make_shared<AIISPPipelineConfig>();
    performance_monitoring_enabled_ = true;
}

bool AIISPPipeline::LoadConfig(const std::string& config_path) {
    // 这里应该从JSON文件加载配置
    // 简化实现：设置默认配置
    pipeline_config_->pipeline_name = "AI_ISP_Default_Pipeline";
    pipeline_config_->input_width = 1920;
    pipeline_config_->input_height = 1080;
    pipeline_config_->input_bit_depth = DataBitDepth::BIT16;
    pipeline_config_->sensor_pattern = "RGGB";
    pipeline_config_->enable_debug_output = false;

    return CreateDefaultPipeline();
}

AIISPResult AIISPPipeline::ProcessFrame(const AIDataBuffer& input) {
    auto start_time = std::chrono::high_resolution_clock::now();

    AIISPResult final_result;

    // 输入验证
    if (input.data == nullptr || input.width == 0 || input.height == 0) {
        final_result.success = false;
        final_result.error_message = "Invalid input data";
        return final_result;
    }

    // 初始化语义数据（如果需要）
    SemanticData semantic_data;

    // 处理各个分支
    std::map<ProcessBranch, AIISPResult> branch_results;
    NNFeatureData accumulated_features;

    for (auto& branch : branches_) {
        if (!branch.second->GetModuleCount()) {
            continue; // 跳过空分支
        }

        AIISPResult branch_result = branch.second->Process(
            input, accumulated_features, semantic_data);

        branch_results[branch.first] = branch_result;

        if (!branch_result.success) {
            final_result = branch_result;
            return final_result;
        }

        // 累积特征数据
        if (!branch_result.nn_features.features.empty()) {
            accumulated_features = branch_result.nn_features;
        }

        // 更新语义数据
        if (!branch_result.semantic_info.segmentation_map.empty()) {
            semantic_data = branch_result.semantic_info;
        }
    }

    // 融合各分支结果
    if (branch_results.size() > 1) {
        final_result = FuseBranchResults(branch_results, semantic_data);
    } else if (!branch_results.empty()) {
        final_result = branch_results.begin()->second;
    } else {
        final_result.success = false;
        final_result.error_message = "No valid branch results";
        return final_result;
    }

    // 计算总处理时间
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    final_result.processing_time_ms = static_cast<double>(duration);

    // 性能监控
    if (performance_monitoring_enabled_) {
        UpdatePerformanceMetrics(final_result);
    }

    return final_result;
}

void AIISPPipeline::AddBranch(std::shared_ptr<BranchProcessor> branch) {
    branches_[branch->GetBranchType()] = branch;
}

std::shared_ptr<AIISPPipelineConfig> AIISPPipeline::GetConfig() const {
    return pipeline_config_;
}

void AIISPPipeline::SetPerformanceMonitoring(bool enable) {
    performance_monitoring_enabled_ = enable;
}

std::map<std::string, double> AIISPPipeline::GetPerformanceStats() const {
    return performance_stats_;
}

void AIISPPipeline::PrintPipelineInfo() const {
    std::cout << "=== AI ISP Pipeline Information ===" << std::endl;
    std::cout << "Pipeline Name: " << pipeline_config_->pipeline_name << std::endl;
    std::cout << "Input Resolution: " << pipeline_config_->input_width << "x"
              << pipeline_config_->input_height << std::endl;
    std::cout << "Input Bit Depth: " << static_cast<int>(pipeline_config_->input_bit_depth)
              << " bits" << std::endl;
    std::cout << "Sensor Pattern: " << pipeline_config_->sensor_pattern << std::endl;
    std::cout << "Number of Branches: " << branches_.size() << std::endl;

    for (const auto& branch : branches_) {
        std::cout << "Branch: " << branch.second->GetBranchName()
                  << " (" << branch.second->GetModuleCount() << " modules)" << std::endl;
    }

    if (performance_monitoring_enabled_) {
        std::cout << "\n=== Performance Statistics ===" << std::endl;
        for (const auto& stat : performance_stats_) {
            std::cout << stat.first << ": " << stat.second << " ms" << std::endl;
        }
    }
    std::cout << "==================================" << std::endl;
}

bool AIISPPipeline::CreateDefaultPipeline() {
    // 这里应该根据配置创建模块链
    // 简化实现：创建空的分支处理器
    auto main_branch = std::make_shared<BranchProcessor>("main_branch", ProcessBranch::MAIN_BRANCH);
    auto ai_branch = std::make_shared<BranchProcessor>("ai_branch", ProcessBranch::AI_BRANCH);
    auto semantic_branch = std::make_shared<BranchProcessor>("semantic_branch", ProcessBranch::SEMANTIC_BRANCH);

    AddBranch(main_branch);
    AddBranch(ai_branch);
    AddBranch(semantic_branch);

    return true;
}

AIISPResult AIISPPipeline::FuseBranchResults(const std::map<ProcessBranch, AIISPResult>& branch_results,
                                  const SemanticData& semantic) {
    AIISPResult final_result;

    // 收集所有分支的输出
    std::vector<AIDataBuffer> branch_outputs;
    std::vector<NNFeatureData> branch_features;

    for (const auto& result : branch_results) {
        if (result.second.success && result.second.output_buffer.data != nullptr) {
            branch_outputs.push_back(result.second.output_buffer);
            if (!result.second.nn_features.features.empty()) {
                branch_features.push_back(result.second.nn_features);
            }
        }
    }

    if (branch_outputs.empty()) {
        final_result.success = false;
        final_result.error_message = "No valid branch outputs to fuse";
        return final_result;
    }

    // 创建融合模块
    SmartMultiBranchFusion fusion;
    fusion.SetAdaptiveFusion(true);

    AIDataBuffer fused_output;
    AIISPResult fusion_result = fusion.FuseMultipleSources(
        branch_outputs, branch_features, semantic, fused_output);

    if (fusion_result.success) {
        final_result = fusion_result;
        final_result.output_buffer = fused_output;

        // 累加处理时间
        for (const auto& result : branch_results) {
            final_result.processing_time_ms += result.second.processing_time_ms;
        }
    } else {
        final_result = fusion_result;
    }

    return final_result;
}

void AIISPPipeline::UpdatePerformanceMetrics(const AIISPResult& result) {
    performance_stats_["total_processing_time"] = result.processing_time_ms;
    performance_stats_["last_frame_time"] = result.processing_time_ms;

    // 计算FPS
    if (result.processing_time_ms > 0) {
        performance_stats_["fps"] = 1000.0 / result.processing_time_ms;
    }
}

} // namespace ai_isp