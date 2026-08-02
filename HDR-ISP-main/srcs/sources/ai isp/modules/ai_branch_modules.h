/**
 * @file ai_branch_modules.h
 * @brief AI处理分支和语义分割分支模块声明
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#ifndef AI_BRANCH_MODULES_H
#define AI_BRANCH_MODULES_H

#include "ai_isp_module.h"

namespace ai_isp {

// AI分支和语义分支模块类声明
class RGBDownsample : public TraditionalISPModule {
public:
    RGBDownsample();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
    void SetDownsampleFactor(int factor);
};

class AIISPEnhancement : public NNModule {
public:
    AIISPEnhancement();
    bool LoadModel(const std::string& model_path) override;
    NNFeatureData Inference(const AIDataBuffer& input) override;
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override;
    void SetEnhancementStrength(float strength);
};

class SemanticInputProcessor : public TraditionalISPModule {
public:
    SemanticInputProcessor();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
};

class NNBasedWarp : public NNModule {
public:
    NNBasedWarp();
    bool LoadModel(const std::string& model_path) override;
    NNFeatureData Inference(const AIDataBuffer& input) override;
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override;
};

} // namespace ai_isp

#endif // AI_BRANCH_MODULES_H