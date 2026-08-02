/**
 * @file test_debug_save.cpp
 * @brief 简单的调试保存功能测试（中文说明）
 *
 * 本文件提供一个简单的测试程序，用于验证调试图像保存功能是否正常工作。
 * 创建一个模拟的帧数据并测试保存功能。
 */

#include "common/pipeline.h"
#include "common/frame.h"
#include "common/image_save.h"
#include "easylogging++.h"
#include <iostream>

INITIALIZE_EASYLOGGINGPP

int main()
{
    // 配置日志系统
    el::Configurations defaultConf;
    defaultConf.setGlobally(el::ConfigurationType::Format, "[%datetime] - %level - %msg");
    defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
    defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
    el::Loggers::reconfigureAllLoggers(defaultConf);

    LOG(INFO) << "=== 调试图像保存功能测试 ===";

    // 测试ImageSaver类
    LOG(INFO) << "测试1: 创建输出目录";
    if (ImageSaver::CreateOutputDirectory("./test_output") == 0) {
        LOG(INFO) << "✓ 目录创建成功";
    } else {
        LOG(ERROR) << "✗ 目录创建失败";
        return -1;
    }

    // 创建测试图像数据
    const int width = 640;
    const int height = 480;
    const int total_pixels = width * height;

    LOG(INFO) << "测试2: 保存RAW图像（uint16）";
    {
        std::vector<uint16_t> raw_data(total_pixels);
        // 创建渐变图案
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                raw_data[y * width + x] = static_cast<uint16_t>((x * 65535) / width);
            }
        }

        if (ImageSaver::SaveRawImage(raw_data.data(), width, height,
                                     DataPtrTypes::TYPE_UINT16,
                                     "./test_output/test_raw.png") == 0) {
            LOG(INFO) << "✓ RAW图像保存成功";
        } else {
            LOG(ERROR) << "✗ RAW图像保存失败";
        }
    }

    LOG(INFO) << "测试3: 保存BGR图像（int32）";
    {
        std::vector<int32_t> bgr_data(total_pixels * 3);
        // 创建彩色渐变图案
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = (y * width + x) * 3;
                bgr_data[idx + 0] = static_cast<int32_t>((x * 255) / width);     // B
                bgr_data[idx + 1] = static_cast<int32_t>((y * 255) / height);    // G
                bgr_data[idx + 2] = static_cast<int32_t>(((x + y) * 255) / (width + height)); // R
            }
        }

        if (ImageSaver::SaveBgrImage(bgr_data.data(), width, height,
                                     DataPtrTypes::TYPE_INT32,
                                     "./test_output/test_bgr.png") == 0) {
            LOG(INFO) << "✓ BGR图像保存成功";
        } else {
            LOG(ERROR) << "✗ BGR图像保存失败";
        }
    }

    LOG(INFO) << "测试4: 保存YUV图像（float）";
    {
        std::vector<float> y_data(total_pixels);
        std::vector<float> u_data(total_pixels);
        std::vector<float> v_data(total_pixels);

        // 创建YUV测试图案
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = y * width + x;
                y_data[idx] = static_cast<float>((x + y) * 1.0f / (width + height));
                u_data[idx] = 0.5f;
                v_data[idx] = 0.5f;
            }
        }

        if (ImageSaver::SaveYuvImage(y_data.data(), u_data.data(), v_data.data(),
                                     width, height, DataPtrTypes::TYPE_FLOAT32,
                                     "./test_output/test_yuv.png") == 0) {
            LOG(INFO) << "✓ YUV图像保存成功";
        } else {
            LOG(ERROR) << "✗ YUV图像保存失败";
        }
    }

    LOG(INFO) << "测试5: 测试IspPipeline调试保存功能";
    {
        // 创建一个简单的流水线
        IspPipeline pipeline;
        pipeline.SetDebugSaveEnabled(true);
        pipeline.SetDebugSavePath("./test_output/pipeline_debug");

        // 创建测试配置
        std::list<std::string> test_pipe = {"unpack", "blc"};
        if (pipeline.MakePipe(test_pipe) == 0) {
            LOG(INFO) << "✓ 测试流水线创建成功";

            // 打印流水线
            pipeline.PrintPipe();

            LOG(INFO) << "流水线调试保存功能已配置";
            LOG(INFO) << "保存路径: ./test_output/pipeline_debug";
            LOG(INFO) << "注意：由于没有实际的帧数据，此测试仅验证配置是否正确";
        } else {
            LOG(ERROR) << "✗ 测试流水线创建失败";
        }
    }

    LOG(INFO) << "=== 测试完成 ===";
    LOG(INFO) << "请检查 ./test_output 目录查看生成的测试图像";
    LOG(INFO) << "生成的文件包括：";
    LOG(INFO) << "  - test_raw.png (RAW数据，伪彩色显示)";
    LOG(INFO) << "  - test_bgr.png (BGR彩色图像)";
    LOG(INFO) << "  - test_yuv.png (YUV转换后的BGR图像)";

    return 0;
}