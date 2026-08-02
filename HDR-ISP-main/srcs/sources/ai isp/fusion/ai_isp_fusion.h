/**
 * @file ai_isp_fusion.h
 * @brief AI ISP多分支数据融合声明
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#ifndef AI_ISP_FUSION_H
#define AI_ISP_FUSION_H

#include "ai_isp_module.h"

namespace ai_isp {

// 融合模块类声明
class SmartMultiBranchFusion : public FusionModule {
public:
    SmartMultiBranchFusion();
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override;
    AIISPResult FuseMultipleSources(
        const std::vector<AIDataBuffer>& inputs,
        const std::vector<NNFeatureData>& features,
        const SemanticData& semantic,
        AIDataBuffer& output) override;
    void SetFusionWeights(const std::map<std::string, float>& weights);
    void SetAdaptiveFusion(bool enable);
};

class SemanticAwareFusion : public FusionModule {
public:
    SemanticAwareFusion();
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override;
    AIISPResult FuseMultipleSources(
        const std::vector<AIDataBuffer>& inputs,
        const std::vector<NNFeatureData>& features,
        const SemanticData& semantic,
        AIDataBuffer& output) override;
    void SetSemanticStrength(uint8_t semantic_class, float strength);
};

} // namespace ai_isp

#endif // AI_ISP_FUSION_H