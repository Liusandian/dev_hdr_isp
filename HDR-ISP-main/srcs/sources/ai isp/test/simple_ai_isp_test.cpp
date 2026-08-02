/**
 * @file simple_ai_isp_test.cpp
 * @brief 简化的AI ISP测试程序 - 快速验证AI ISP模块功能
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#include "ai_isp_module.h"
#include "ai_isp_types.h"
#include "main_path_modules.h"
#include "ai_branch_modules.h"
#include "ai_isp_fusion.h"
#include "ai_isp_pipeline.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <random>
#include <cstring>
#include <fstream>

using namespace ai_isp;

/**
 * @brief 生成简单的测试RAW数据
 */
AIDataBuffer GenerateSimpleTestRAW(size_t width, size_t height) {
    AIDataBuffer buffer;
    buffer.width = width;
    buffer.height = height;
    buffer.channels = 1;
    buffer.domain = DataDomain::RAW_DOMAIN;
    buffer.bit_depth = DataBitDepth::BIT16;
    buffer.size = width * height * sizeof(uint16_t);
    buffer.buffer_name = "simple_test_raw";
    buffer.data = new uint8_t[buffer.size];

    uint16_t* raw_data = static_cast<uint16_t*>(buffer.data);

    // 生成简单的渐变测试图样
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            // 创建对角线渐变
            float gradient = static_cast<float>(x + y) / (width + height) * 40000.0f + 10000.0f;
            raw_data[y * width + x] = static_cast<uint16_t>(gradient);
        }
    }

    return buffer;
}

/**
 * @brief 将float RGB数据保存为简单的文本格式（用于调试）
 */
void SaveRGBToText(const AIDataBuffer& rgb_buffer, const std::string& filename, size_t max_pixels = 100) {
    if (rgb_buffer.data == nullptr || rgb_buffer.channels != 3) {
        std::cerr << "Invalid RGB buffer" << std::endl;
        return;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    file << "RGB Data Dump (" << rgb_buffer.width << "x" << rgb_buffer.height << ")\n";
    file << "Showing first " << max_pixels << " pixels:\n\n";

    float* rgb_data = static_cast<float*>(rgb_buffer.data);
    size_t pixels_to_show = std::min(max_pixels, rgb_buffer.width * rgb_buffer.height);

    for (size_t i = 0; i < pixels_to_show; ++i) {
        size_t idx = i * 3;
        file << "Pixel[" << i << "]: R=" << std::fixed << std::setprecision(3) << rgb_data[idx]
              << ", G=" << rgb_data[idx + 1] << ", B=" << rgb_data[idx + 2] << "\n";
    }

    file.close();
    std::cout << "RGB data saved to: " << filename << std::endl;
}

/**
 * @brief 测试单个模块
 */
bool TestSingleModule(AIISPModulePtr module, const AIDataBuffer& input, const std::string& module_name) {
    std::cout << "\n--- Testing " << module_name << " ---" << std::endl;

    AIDataBuffer output;
    auto start = std::chrono::high_resolution_clock::now();

    AIISPResult result = module->Process(input, output);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (result.success) {
        std::cout << "✓ " << module_name << " completed successfully" << std::endl;
        std::cout << "  Processing time: " << duration << " ms" << std::endl;
        std::cout << "  Output: " << output.width << "x" << output.height
                  << ", " << output.channels << " channels" << std::endl;

        // 清理输出内存
        if (output.data != nullptr && output.data != input.data) {
            delete[] static_cast<uint8_t*>(output.data);
        }
        return true;
    } else {
        std::cout << "✗ " << module_name << " failed: " << result.error_message << std::endl;
        return false;
    }
}

/**
 * @brief 创建简化的AI ISP Pipeline
 */
std::shared_ptr<AIISPPipeline> CreateSimplePipeline() {
    auto pipeline = std::make_shared<AIISPPipeline>();

    // 创建主处理分支
    auto main_branch = std::make_shared<BranchProcessor>("main", ProcessBranch::MAIN_BRANCH);
    main_branch->AddModule(std::make_shared<RawFrontEnd>());
    main_branch->AddModule(std::make_shared<NNFeatureExtractor>());
    main_branch->AddModule(std::make_shared<RGBLSC>());
    main_branch->AddModule(std::make_shared<GammaCorrection>());

    // 创建AI分支
    auto ai_branch = std::make_shared<BranchProcessor>("ai", ProcessBranch::AI_BRANCH);
    ai_branch->AddModule(std::make_shared<RGBDownsample>());
    ai_branch->AddModule(std::make_shared<AIISPEnhancement>());

    pipeline->AddBranch(main_branch);
    pipeline->AddBranch(ai_branch);

    return pipeline;
}

/**
 * @brief 主测试函数
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    Simple AI ISP Test Program        " << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // 测试参数
        size_t test_width = 320;
        size_t test_height = 240;

        std::cout << "\nTest Configuration:" << std::endl;
        std::cout << "  Resolution: " << test_width << "x" << test_height << std::endl;
        std::cout << "  Format: RAW16" << std::endl;

        // 1. 生成测试数据
        std::cout << "\n[1/6] Generating test data..." << std::endl;
        AIDataBuffer test_input = GenerateSimpleTestRAW(test_width, test_height);
        std::cout << "✓ Test data generated successfully" << std::endl;

        // 2. 测试单个模块
        std::cout << "\n[2/6] Testing individual modules..." << std::endl;

        int passed_tests = 0;
        int total_tests = 0;

        // 测试主处理模块
        total_tests++;
        if (TestSingleModule(std::make_shared<RawFrontEnd>(), test_input, "RawFrontEnd")) {
            passed_tests++;
        }

        // 测试AI处理模块（需要RGB输入，先转换）
        AIDataBuffer rgb_test;
        rgb_test.width = test_width;
        rgb_test.height = test_height;
        rgb_test.channels = 3;
        rgb_test.domain = DataDomain::RGB_DOMAIN;
        rgb_test.bit_depth = DataBitDepth::BIT8;
        rgb_test.size = test_width * test_height * 3 * sizeof(float);
        rgb_test.data = new uint8_t[rgb_test.size];
        rgb_test.buffer_name = "test_rgb";

        // 生成简单的RGB测试数据
        float* rgb_data = static_cast<float*>(rgb_test.data);
        for (size_t i = 0; i < test_width * test_height * 3; ++i) {
            rgb_data[i] = 0.5f; // 中性灰色
        }

        total_tests++;
        if (TestSingleModule(std::make_shared<RGBDownsample>(), rgb_test, "RGBDownsample")) {
            passed_tests++;
        }

        total_tests++;
        if (TestSingleModule(std::make_shared<AIISPEnhancement>(), rgb_test, "AIISPEnhancement")) {
            passed_tests++;
        }

        std::cout << "\nModule Tests: " << passed_tests << "/" << total_tests << " passed" << std::endl;

        // 3. 测试完整Pipeline
        std::cout << "\n[3/6] Testing complete Pipeline..." << std::endl;
        auto pipeline = CreateSimplePipeline();

        auto pipeline_start = std::chrono::high_resolution_clock::now();
        AIISPResult pipeline_result = pipeline->ProcessFrame(test_input);
        auto pipeline_end = std::chrono::high_resolution_clock::now();

        auto pipeline_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            pipeline_end - pipeline_start).count();

        if (pipeline_result.success) {
            std::cout << "✓ Pipeline completed successfully" << std::endl;
            std::cout << "  Total processing time: " << pipeline_duration << " ms" << std::endl;
            std::cout << "  Output dimensions: " << pipeline_result.output_buffer.width << "x"
                      << pipeline_result.output_buffer.height << std::endl;

            // 保存输出样本
            if (pipeline_result.output_buffer.data != nullptr) {
                std::cout << "\n[4/6] Saving output samples..." << std::endl;
                SaveRGBToText(pipeline_result.output_buffer, "ai_isp_output_samples.txt", 50);
            }
        } else {
            std::cout << "✗ Pipeline failed: " << pipeline_result.error_message << std::endl;
        }

        // 4. 测试融合模块
        std::cout << "\n[5/6] Testing fusion modules..." << std::endl;

        SmartMultiBranchFusion fusion;
        std::vector<AIDataBuffer> test_inputs = {rgb_test, rgb_test}; // 两个相同输入用于测试
        AIDataBuffer fused_output;

        AIISPResult fusion_result = fusion.FuseMultipleSources(
            test_inputs, {}, {}, fused_output);

        if (fusion_result.success) {
            std::cout << "✓ Fusion module completed successfully" << std::endl;
            if (fused_output.data != nullptr) {
                delete[] static_cast<uint8_t*>(fused_output.data);
            }
        } else {
            std::cout << "✗ Fusion module failed: " << fusion_result.error_message << std::endl;
        }

        // 5. 性能基准测试
        std::cout << "\n[6/6] Running performance benchmark..." << std::endl;
        const int benchmark_iterations = 5;
        std::vector<double> processing_times;

        for (int i = 0; i < benchmark_iterations; ++i) {
            auto iter_start = std::chrono::high_resolution_clock::now();
            AIISPResult iter_result = pipeline->ProcessFrame(test_input);
            auto iter_end = std::chrono::high_resolution_clock::now();

            auto iter_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                iter_end - iter_start).count();

            if (iter_result.success) {
                processing_times.push_back(static_cast<double>(iter_duration));
            }

            // 清理
            if (iter_result.output_buffer.data != nullptr) {
                delete[] static_cast<uint8_t*>(iter_result.output_buffer.data);
            }

            std::cout << "  Iteration " << (i + 1) << ": " << iter_duration << " ms" << std::endl;
        }

        if (!processing_times.empty()) {
            double avg_time = 0.0;
            for (double time : processing_times) {
                avg_time += time;
            }
            avg_time /= processing_times.size();
            double fps = 1000.0 / avg_time;

            std::cout << "\n=== Performance Summary ===" << std::endl;
            std::cout << "Average processing time: " << std::fixed << std::setprecision(2) << avg_time << " ms" << std::endl;
            std::cout << "Throughput: " << fps << " FPS" << std::endl;
        }

        // 6. 清理资源
        std::cout << "\nCleaning up resources..." << std::endl;
        delete[] static_cast<uint8_t*>(test_input.data);
        delete[] static_cast<uint8_t*>(rgb_test.data);
        if (pipeline_result.output_buffer.data != nullptr) {
            delete[] static_cast<uint8_t*>(pipeline_result.output_buffer.data);
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "    All Tests Completed!            " << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}