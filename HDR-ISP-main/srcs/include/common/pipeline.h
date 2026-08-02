/**
 * @file pipeline.h
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */

#ifndef ISP_PIPE_H
#define ISP_PIPE_H

#include <list>
#include <functional>

#include "modules/modules.h"
#include <chrono>

class IspPipeline
{
private:
    /* data */
    std::list<IspModule> pipe_;
    bool is_pipe_vaild_ = false;

    // 调试图像保存相关
    bool debug_save_enabled_ = false;
    std::string debug_save_path_ = "./debug_output";
    int frame_count_ = 0;

    /**
     * @brief 保存调试图像的内部函数
     *
     * @param frame 帧对象
     * @param module_name 模块名称
     * @param stage 阶段（"input"或"output"）
     * @param data_type 数据类型
     * @param domain 颜色域
     */
    void SaveDebugImage(Frame *frame, const std::string &module_name,
                       const std::string &stage, DataPtrTypes data_type,
                       ColorDomains domain);

public:
    IspPipeline();
    ~IspPipeline();

    IspPipeline(std::list<std::string> pipeline);

    int MakePipe(const std::list<std::string> &pipeline_str, const IspPrms *prms = nullptr);
    int RunPipe(Frame *frame, const IspPrms *prms);
    int PrintPipe();

    /**
     * @brief 启用或禁用调试图像保存
     *
     * @param enabled true启用，false禁用
     */
    void SetDebugSaveEnabled(bool enabled) { debug_save_enabled_ = enabled; }

    /**
     * @brief 设置调试图像保存路径
     *
     * @param path 保存路径
     */
    void SetDebugSavePath(const std::string &path) { debug_save_path_ = path; }

    /**
     * @brief 重置帧计数器（用于文件命名）
     */
    void ResetFrameCount() { frame_count_ = 0; }
};


#endif // ! ISP_PIPE_H