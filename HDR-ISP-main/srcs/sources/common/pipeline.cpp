/**
 * @file pipeline.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 * @brief ISP 流水线（Pipeline）核心实现（中文说明）
 *
 * 本文件实现 IspPipeline 类，是整个 HDR-ISP 的调度核心。
 * 它把若干个 ISP 模块（BLC、Demosaic、CCM、Gamma、LTM 等）按顺序串联，
 * 依次对一帧图像做处理。
 *
 * 工作流程：
 *   1. 构造时调用 IspInit() 注册所有内置模块
 *   2. MakePipe() 接收一组模块名（字符串列表），做域/位宽一致性校验，
 *      校验通过后构建可执行模块链表 pipe_
 *   3. RunPipe() 按 pipe_ 顺序逐个调用模块的 run_function，
 *      并统计每个模块的耗时
 *   4. PrintPipe() 打印当前流水线组成
 */
#include "modules/modules.h"
#include "common/pipeline.h"
// #include "common/image_save.h" // 暂时禁用调试保存功能
#include <sstream>

/**
 * @brief 注册所有内置 ISP 模块到全局注册表
 *
 * 该函数在 IspPipeline 构造时被调用一次，把内置算法模块全部注册。
 * 每个 RegisterXxxMod() 内部会构造 IspModule 并调用 RegisterIspModule。
 *
 * 注意：当前实现中 RegisterContrastMod() 被调用了两次（疑似笔误，
 * 第二次会覆盖第一次，不会造成功能错误但属冗余）。
 */
void IspInit()
{
    RegisterUnpackMod();      // RAW 打包数据解包（MIPI packed -> raw16）
    RegisterDePwlMod();       // 反分段线性化（De-PWL，HDR 压缩逆变换）
    RegisterBlcMod();         // 黑电平校正（Black Level Correction）
    RegisterDemoasicMod();    // 去马赛克（Demosaic，Bayer -> RGB）
    RegisterCcmMod();         // 色彩校正矩阵（Color Correction Matrix）
    RegisterYGammaMod();      // Y 通道 Gamma 校正
    RegisterWbGaincMod();     // 白平衡增益（White Balance Gain）
    RegisterLtmMod();         // 局部色调映射（Local Tone Mapping）
    RegisterRgbGammaMod();    // RGB 通道 Gamma 校正
    RegisterYuv2RgbMod();     // YUV 转 RGB
    RegisterRgb2YuvMod();     // RGB 转 YUV
    RegisterSaturationMod();  // 饱和度调整
    RegisterContrastMod();    // 对比度调整
    RegisterContrastMod();    // 注意：此处重复注册，疑似笔误
    RegisterSharpenMod();     // 锐化（Sharpen）
    RegisterLscMod();         // 镜头阴影校正（Lens Shading Correction）
    RegisterDpcMod();         // 坏点校正（Defective Pixel Correction）
    RegisterCnsMod();         // 色度噪声抑制（Chroma Noise Suppression）
}

/**
 * @brief 默认构造函数：清空流水线并注册所有内置模块
 *
 * 调用 IspInit() 完成模块注册，再调用 ShowAllIspModules() 打印
 * 当前可用模块列表，便于调试与确认注册成功。
 */
IspPipeline::IspPipeline()
{
    pipe_.clear();
    IspInit();
    ShowAllIspModules();
}

/**
 * @brief 析构函数：清空流水线链表，释放链表节点资源
 *        注意：Frame 内存由 Frame 类自己管理，pipeline 不负责释放。
 */
IspPipeline::~IspPipeline()
{
    pipe_.clear();
}

/**
 * @brief 带初始模块列表的构造函数
 *
 * @param pipeline 模块名列表（如 {"blc", "demosaic", "ccm", ...}）
 *
 * 注意：当前实现先调用无参构造 IspPipeline()（但这只是构造一个临时对象，
 * 不会真正初始化本对象，这是 C++ 的一个常见陷阱），再调用 MakePipe。
 * 推荐改为在成员初始化列表中调用，或显式调用 IspInit()。
 */
IspPipeline::IspPipeline(std::list<std::string> pipeline)
{
    IspPipeline();
    MakePipe(pipeline);
}

/**
 * @brief 根据模块名列表构建可执行流水线
 *
 * @param pipeline_str 模块名列表（按处理顺序排列）
 * @return int  0 构建成功；-1 模块域或位宽不匹配
 *
 * 关键校验逻辑：
 *   - 相邻模块必须满足：前一个模块的 out_type == 后一个模块的 in_type
 *     （位宽匹配，例如 16bit 输出不能直接接 8bit 输入模块）
 *   - 相邻模块必须满足：前一个模块的 out_domain == 后一个模块的 in_domain
 *     （域匹配，例如 RAW 域输出不能直接接 YUV 域处理模块）
 *   - 任一校验失败都会标记 is_pipe_vaild_ = false，RunPipe 时会拒绝执行
 */
int IspPipeline::MakePipe(const std::list<std::string> &pipeline_str, const IspPrms *prms)
{
    IspModule mod;
    IspModule last_mod;

    // 从配置中获取是否启用严格验证（新增）
    bool enable_strict_validation = true;
    if (prms != nullptr && prms->pipeline_config.enable_strict_validation) {
        enable_strict_validation = prms->pipeline_config.enable_strict_validation;
    }

    for (auto item : pipeline_str)
    {
        if (0 == GetIspModuleFromName(item, mod))
        {
            // 已有前驱模块时，根据配置决定是否做位宽与域的一致性校验
            if (pipe_.size() > 0 && enable_strict_validation)
            {
                // 校验：前一模块输出位宽/域 必须等于 当前模块输入位宽/域
                if ((mod.in_type != last_mod.out_type) || (mod.in_domain != last_mod.out_domain))
                {
                    // 域不匹配（如 RAW 直接连 YUV 处理模块）—— 严重错误
                    if ((mod.in_domain != last_mod.out_domain))
                        LOG(ERROR) << "mod " << mod.name << " domain is not equal wait " << last_mod.name;
                    // 位宽不匹配（如 16bit 输出接 8bit 输入）—— 严重错误
                    if (mod.in_type != last_mod.out_type)
                        LOG(ERROR) << "mod " << mod.name << " in bit is not equal wait " << last_mod.name;
                    is_pipe_vaild_ = false;
                    return -1;
                }
            }
            // 校验通过，加入流水线
            pipe_.push_back(mod);
            last_mod = mod;
        }
        else
        {
            // 模块名在注册表中查找不到，给出告警但继续处理后续模块名
            LOG(WARNING) << item << " find failed";
        }
    }
    is_pipe_vaild_ = true;
    return 0;
}

/**
 * @brief 保存调试图像的内部函数实现
 *
 * @param frame 帧对象
 * @param module_name 模块名称
 * @param stage 阶段（"input"或"output"）
 * @param data_type 数据类型
 * @param domain 颜色域
 */
void IspPipeline::SaveDebugImage(Frame *frame, const std::string &module_name,
                                const std::string &stage, DataPtrTypes data_type,
                                ColorDomains domain)
{
    if (!debug_save_enabled_) {
        return;
    }

    // 暂时禁用调试保存功能
    /*
    // 创建输出目录
    ImageSaver::CreateOutputDirectory(debug_save_path_);

    // 生成文件名：frame_模块名_阶段.png
    std::ostringstream filename;
    filename << debug_save_path_ << "/frame_" << frame_count_ << "_"
             << module_name << "_" << stage << ".png";

    // 保存图像
    ImageSaver::SaveFrame(frame, filename.str(), data_type, domain);
    */
}

/**
 * @brief 运行整条流水线，对一帧图像做完整 ISP 处理
 *
 * @param frame 待处理的帧（含输入数据，处理结果也写回该 frame）
 * @param prms  各模块参数集合（IspPrms 指针，由调用方准备）
 * @return int  0 全部模块执行成功；-1 流水线无效或某模块执行失败
 *
 * 执行流程：
 *   1. 校验流水线是否有效（is_pipe_vaild_）
 *   2. 遍历 pipe_ 中的每个模块，依次调用其 run_function
 *   3. 使用 std::chrono 统计每个模块的执行耗时（毫秒），用于性能分析
 *   4. 任一模块返回非 0 即视为失败，立即终止并返回 -1
 *   5. 如果启用调试保存，在每个模块执行前后保存输入输出图像
 */
int IspPipeline::RunPipe(Frame *frame, const IspPrms *prms)
{
    if (!is_pipe_vaild_)
    {
        LOG(ERROR) << "pipeline is not vailed..";
        return -1;
    }

    // 从配置中获取控制参数（新增）
    bool enable_module_timing = true;
    bool stop_on_first_error = true;
    bool print_pipeline_on_start = true;
    bool print_pipeline_on_end = true;

    if (prms != nullptr) {
        enable_module_timing = prms->pipeline_config.enable_module_timing;
        stop_on_first_error = prms->pipeline_config.stop_on_first_error;
        print_pipeline_on_start = prms->pipeline_config.print_pipeline_on_start;
        print_pipeline_on_end = prms->pipeline_config.print_pipeline_on_end;
    }

    // 根据配置决定是否打印流水线开始信息
    if (print_pipeline_on_start) {
        LOG(INFO) << "============= user pipeline running ==============";
    }

    // 如果启用调试保存，创建输出目录并增加帧计数
    if (debug_save_enabled_) {
        // ImageSaver::CreateOutputDirectory(debug_save_path_); // 暂时禁用
        if (print_pipeline_on_start) {
            LOG(INFO) << "Debug save enabled (but temporarily disabled due to OpenCV dependency)";
        }
    }

    for (auto isp_mod : pipe_)
    {
        // 记录模块开始时间（自 epoch 起的毫秒数）
        auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

        // 保存模块输入图像（如果启用调试保存）
        if (debug_save_enabled_) {
            if (print_pipeline_on_start) {
                LOG(INFO) << "Saving input image for module: " << isp_mod.name;
            }
            SaveDebugImage(frame, isp_mod.name, "input", isp_mod.in_type, isp_mod.in_domain);
        }

        // 调用模块的处理函数，传入帧与参数
        if (isp_mod.run_function(frame, prms) != 0)
        {
            LOG(ERROR) << "pipeline run failed, mod " << isp_mod.name;

            // 根据配置决定是否在遇到错误时立即停止
            if (stop_on_first_error) {
                if (print_pipeline_on_end) {
                    LOG(INFO) << "============= user pipeline running end (with errors) ==============";
                }
                return -1;
            } else {
                LOG(WARNING) << "mod " << isp_mod.name << " failed but continuing due to stop_on_first_error=false";
            }
        }

        // 保存模块输出图像（如果启用调试保存）
        if (debug_save_enabled_) {
            if (print_pipeline_on_start) {
                LOG(INFO) << "Saving output image for module: " << isp_mod.name;
            }
            SaveDebugImage(frame, isp_mod.name, "output", isp_mod.out_type, isp_mod.out_domain);
        }

        // 根据配置决定是否记录和打印模块执行时间
        if (enable_module_timing) {
            // 记录模块结束时间，计算并打印耗时
            auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
            LOG(INFO) << "mod " << isp_mod.name << "\t time: " << (end_ms - start_ms).count() << "ms";
        }
    }

    // 增加帧计数
    if (debug_save_enabled_) {
        frame_count_++;
    }

    // 根据配置决定是否打印流水线结束信息
    if (print_pipeline_on_end) {
        LOG(INFO) << "============= user pipeline running end ==============";
    }

    return 0;
}

/**
 * @brief 打印当前用户流水线的组成（模块顺序与名称）
 *
 * @return int  0 成功；-1 流水线无效
 *
 * 用于调试与确认 MakePipe 后流水线是否符合预期。
 */
int IspPipeline::PrintPipe()
{
    if (!is_pipe_vaild_)
    {
        LOG(ERROR) << "pipeline is not vailed..";
        return -1;
    }
    int index = 0;
    LOG(INFO) << "============= user pipeline print start ==============";
    for (auto isp_mod : pipe_)
    {
        LOG(INFO) << "mod[" << index++ << "] -> " << isp_mod.name;
    }
    LOG(INFO) << "============= user pipeline print end ==============";
    return 0;
}