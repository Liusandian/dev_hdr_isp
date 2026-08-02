/**
 * @file main_path_modules.h
 * @brief 主处理路径模块声明
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#ifndef MAIN_PATH_MODULES_H
#define MAIN_PATH_MODULES_H

#include "ai_isp_module.h"

namespace ai_isp {

// 主处理路径模块类声明
class RawFrontEnd : public TraditionalISPModule {
public:
    RawFrontEnd();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
};

class NNFeatureExtractor : public NNModule {
public:
    NNFeatureExtractor();
    bool LoadModel(const std::string& model_path) override;
    NNFeatureData Inference(const AIDataBuffer& input) override;
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override;
};

class RGBLSC : public TraditionalISPModule {
public:
    RGBLSC();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
};

class RGBDRC : public TraditionalISPModule {
public:
    RGBDRC();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
};

class ColorCorrectionMatrix : public TraditionalISPModule {
public:
    ColorCorrectionMatrix();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
};

class GammaCorrection : public TraditionalISPModule {
public:
    GammaCorrection();
    AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) override;
    void SetGamma(float gamma);
};

} // namespace ai_isp

#endif // MAIN_PATH_MODULES_H