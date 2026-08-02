/**
 * @file debug_save_example.cpp
 * @brief ISP调试图像保存功能使用示例（中文说明）
 *
 * 本文件演示如何使用ISP流水线的调试图像保存功能，
 * 在各个处理模块（如CCM、LTM等）执行前后保存输入输出图像。
 *
 * 使用方法：
 *   1. 创建IspPipeline对象
 *   2. 启用调试保存功能：SetDebugSaveEnabled(true)
 *   3. 设置保存路径：SetDebugSavePath("./debug_output")
 *   4. 构建流水线：MakePipe()
 *   5. 运行流水线：RunPipe()
 *   6. 在指定目录查看保存的图像
 */

#include "common/pipeline.h"
#include "common/frame.h"
#include "modules/modules.h"
#include "common/parse.h"
#include "easylogging++.h"
#include <iostream>

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[])
{
    // 配置日志系统
    el::Configurations defaultConf;
    defaultConf.setGlobally(el::ConfigurationType::Format, "[%datetime] - %level - %msg");
    defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
    defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
    el::Loggers::reconfigureAllLoggers(defaultConf);

    LOG(INFO) << "ISP调试图像保存功能示例";

    // 检查命令行参数
    if (argc < 2) {
        LOG(ERROR) << "Usage: " << argv[0] << " <config_file>";
        return -1;
    }

    std::string config_file = argv[1];
    LOG(INFO) << "Loading config from: " << config_file;

    // 解析配置文件
    IspPrms prms;
    if (ParseConfig(config_file, &prms) != 0) {
        LOG(ERROR) << "Failed to parse config file";
        return -1;
    }

    // 创建ISP流水线
    IspPipeline pipeline;

    // === 启用调试图像保存功能 ===
    pipeline.SetDebugSaveEnabled(true);
    pipeline.SetDebugSavePath("./debug_output");  // 设置保存路径
    pipeline.ResetFrameCount();                   // 重置帧计数器

    LOG(INFO) << "Debug image saving enabled. Images will be saved to: ./debug_output";

    // 构建流水线
    if (pipeline.MakePipe(prms.pipe) != 0) {
        LOG(ERROR) << "Failed to make pipeline";
        return -1;
    }

    // 打印流水线信息
    pipeline.PrintPipe();

    // 创建帧对象
    Frame frame(prms.info);

    // 读取RAW文件
    if (frame.ReadFileToFrame(prms.raw_file, 0) != 0) {
        LOG(ERROR) << "Failed to read RAW file";
        return -1;
    }

    LOG(INFO) << "Running pipeline with debug save enabled...";

    // 运行流水线（会自动保存每个模块的输入输出图像）
    if (pipeline.RunPipe(&frame, &prms) != 0) {
        LOG(ERROR) << "Pipeline execution failed";
        return -1;
    }

    LOG(INFO) << "Pipeline completed successfully!";
    LOG(INFO) << "Debug images saved to: ./debug_output";
    LOG(INFO) << "Image naming convention: frame_<frame_number>_<module_name>_<input|output>.png";
    LOG(INFO) << "Example: frame_0_ccm_input.png, frame_0_ccm_output.png, frame_0_ltm_input.png, etc.";

    return 0;
}

/**
 * @brief 配置文件示例 (config.json)
 *
 * {
 *   "raw_file": "input.raw",
 *   "out_file_path": "output.png",
 *   "sensor_name": "sensor_name",
 *   "blc": 64,
 *   "data_packed": true,
 *   "pipe": ["unpack", "blc", "demosaic", "ccm", "ltm", "rgb_gamma"],
 *   "info": {
 *     "width": 1920,
 *     "height": 1080,
 *     "bpp": 16,
 *     "max_val": 1023,
 *     "mipi_packed": true,
 *     "cfa": 0,
 *     "domain": 0,
 *     "dt": 4,
 *     "yuv_type": 0
 *   }
 * }
 *
 * @brief 输出文件说明
 *
 * 启用调试保存后，会在指定目录生成以下文件：
 *
 * 1. RAW处理阶段：
 *    - frame_0_unpack_input.png: 解包前的RAW数据
 *    - frame_0_unpack_output.png: 解包后的RAW数据
 *
 * 2. 黑电平校正：
 *    - frame_0_blc_input.png: BLC处理前的图像
 *    - frame_0_blc_output.png: BLC处理后的图像
 *
 * 3. 去马赛克：
 *    - frame_0_demosaic_input.png: Bayer格式的RAW图像
 *    - frame_0_demosaic_output.png: 去马赛克后的RGB图像
 *
 * 4. 色彩校正矩阵（CCM）：
 *    - frame_0_ccm_input.png: CCM处理前的RGB图像
 *    - frame_0_ccm_output.png: CCM处理后的RGB图像（色彩校正效果）
 *
 * 5. 局部色调映射（LTM）：
 *    - frame_0_ltm_input.png: LTM处理前的图像
 *    - frame_0_ltm_output.png: LTM处理后的图像（局部对比度增强效果）
 *
 * 6. Gamma校正：
 *    - frame_0_rgb_gamma_input.png: Gamma校正前的图像
 *    - frame_0_rgb_gamma_output.png: Gamma校正后的图像
 *
 * 注意：
 * - RAW图像使用伪彩色（COLORMAP_JET）显示以便观察
 * - BGR图像直接保存为彩色图像
 * - YUV图像会转换为BGR格式保存
 * - 文件名包含帧编号、模块名和输入输出阶段
 */