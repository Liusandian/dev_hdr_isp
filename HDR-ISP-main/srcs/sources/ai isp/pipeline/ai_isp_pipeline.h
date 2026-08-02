/**
 * @file ai_isp_pipeline.h
 * @brief AI ISP Pipeline管理系统声明
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#ifndef AI_ISP_PIPELINE_H
#define AI_ISP_PIPELINE_H

#include "ai_isp_module.h"

namespace ai_isp {

/**
 * @brief 分支处理器
 * 管理单个处理分支的模块链
 */
class BranchProcessor {
public:
    BranchProcessor(const std::string& name, ProcessBranch branch_type);
    void AddModule(AIISPModulePtr module);
    AIISPResult Process(const AIDataBuffer& input,
                       const NNFeatureData& features,
                       const SemanticData& semantic);
    std::string GetBranchName() const;
    ProcessBranch GetBranchType() const;
    size_t GetModuleCount() const;

private:
    std::string branch_name_;
    ProcessBranch branch_type_;
    std::vector<AIISPModulePtr> modules_;
};

// Pipeline管理类声明
class AIISPPipeline {
public:
    AIISPPipeline();
    bool LoadConfig(const std::string& config_path);
    AIISPResult ProcessFrame(const AIDataBuffer& input);
    void AddBranch(std::shared_ptr<BranchProcessor> branch);
    std::shared_ptr<AIISPPipelineConfig> GetConfig() const;
    void SetPerformanceMonitoring(bool enable);
    std::map<std::string, double> GetPerformanceStats() const;
    void PrintPipelineInfo() const;

private:
    std::shared_ptr<AIISPPipelineConfig> pipeline_config_;
    std::map<ProcessBranch, std::shared_ptr<BranchProcessor>> branches_;
    bool performance_monitoring_enabled_;
    std::map<std::string, double> performance_stats_;

    bool CreateDefaultPipeline();
    AIISPResult FuseBranchResults(const std::map<ProcessBranch, AIISPResult>& branch_results,
                                  const SemanticData& semantic);
    void UpdatePerformanceMetrics(const AIISPResult& result);
};

} // namespace ai_isp

#endif // AI_ISP_PIPELINE_H