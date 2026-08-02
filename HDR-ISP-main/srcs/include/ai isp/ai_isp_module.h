/**
 * @file ai_isp_module.h
 * @brief AI ISP模块基类定义
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#ifndef AI_ISP_MODULE_H
#define AI_ISP_MODULE_H

#include "ai_isp_types.h"
#include <memory>
#include <chrono>
#include <string>

namespace ai_isp {

/**
 * @brief AI ISP模块基类
 * 所有AI ISP处理模块都继承自这个基类
 */
class AIISPModule {
public:
    explicit AIISPModule(const std::string& name) : module_name_(name), enable_(true) {}
    virtual ~AIISPModule() = default;

    /**
     * @brief 初始化模块
     */
    virtual bool Initialize(const AIModuleConfig& config) {
        config_ = config;
        return true;
    }

    /**
     * @brief 处理数据
     */
    virtual AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) = 0;

    /**
     * @brief 处理数据（带特征输入）
     */
    virtual AIISPResult ProcessWithFeatures(const AIDataBuffer& input,
                                           const NNFeatureData& features,
                                           AIDataBuffer& output) {
        // 默认实现：忽略特征，调用普通处理
        return Process(input, output);
    }

    /**
     * @brief 处理数据（带语义信息）
     */
    virtual AIISPResult ProcessWithSemantic(const AIDataBuffer& input,
                                           const SemanticData& semantic,
                                           AIDataBuffer& output) {
        // 默认实现：忽略语义信息，调用普通处理
        return Process(input, output);
    }

    /**
     * @brief 获取模块信息
     */
    std::string GetModuleName() const { return module_name_; }
    AIModuleConfig GetConfig() const { return config_; }
    bool IsEnabled() const { return enable_; }
    void SetEnable(bool enable) { enable_ = enable; }

protected:
    std::string module_name_;
    AIModuleConfig config_;
    bool enable_;

    // 辅助计时函数
    double StartTimer() {
        start_time_ = std::chrono::high_resolution_clock::now();
        return 0.0;
    }

    double EndTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_).count();
        return duration / 1000.0; // 转换为毫秒
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
};

/**
 * @brief 神经网络处理模块基类
 */
class NNModule : public AIISPModule {
public:
    explicit NNModule(const std::string& name) : AIISPModule(name) {}

    /**
     * @brief 加载神经网络模型
     */
    virtual bool LoadModel(const std::string& model_path) = 0;

    /**
     * @brief 推理
     */
    virtual NNFeatureData Inference(const AIDataBuffer& input) = 0;

protected:
    std::string model_path_;
    bool model_loaded_ = false;
};

/**
 * @brief 传统ISP处理模块基类
 */
class TraditionalISPModule : public AIISPModule {
public:
    explicit TraditionalISPModule(const std::string& name) : AIISPModule(name) {}

    /**
     * @brief 处理单帧数据
     */
    virtual AIISPResult ProcessSingleFrame(const AIDataBuffer& input, AIDataBuffer& output) = 0;

    /**
     * @brief 实现基类处理接口
     */
    AIISPResult Process(const AIDataBuffer& input, AIDataBuffer& output) override {
        StartTimer();
        AIISPResult result = ProcessSingleFrame(input, output);
        result.processing_time_ms = EndTimer();
        return result;
    }
};

/**
 * @brief 融合模块基类
 */
class FusionModule : public AIISPModule {
public:
    explicit FusionModule(const std::string& name) : AIISPModule(name) {}

    /**
     * @brief 融合多个数据源
     */
    virtual AIISPResult FuseMultipleSources(
        const std::vector<AIDataBuffer>& inputs,
        const std::vector<NNFeatureData>& features,
        const SemanticData& semantic,
        AIDataBuffer& output) = 0;

    /**
     * @brief 融合策略设置
     */
    virtual void SetFusionStrategy(const std::string& strategy) {
        fusion_strategy_ = strategy;
    }

protected:
    std::string fusion_strategy_ = "weighted";
};

// 智能指针类型定义
using AIISPModulePtr = std::shared_ptr<AIISPModule>;
using NNModulePtr = std::shared_ptr<NNModule>;
using TraditionalISPModulePtr = std::shared_ptr<TraditionalISPModule>;
using FusionModulePtr = std::shared_ptr<FusionModule>;

} // namespace ai_isp

#endif // AI_ISP_MODULE_H