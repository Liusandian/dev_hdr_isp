/**
 * @file ai_isp_test.cpp
 * @brief AI ISP系统测试和验证程序
 * @author AI ISP Team
 * @version 1.0
 * @date 2026-08-02
 */

#include "ai_isp_module.h"
#include "ai_isp_types.h"
#include "ai_isp_pipeline.h"
#include "main_path_modules.h"
#include "ai_branch_modules.h"
#include "ai_isp_fusion.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cstring>

// 前向声明模块类
namespace ai_isp {
    class RawFrontEnd;
    class NNFeatureExtractor;
    class RGBLSC;
    class RGBDRC;
    class ColorCorrectionMatrix;
    class GammaCorrection;
    class RGBDownsample;
    class AIISPEnhancement;
    class SemanticInputProcessor;
    class NNBasedWarp;
    class SmartMultiBranchFusion;
}

using namespace ai_isp;

/**
 * @brief 生成测试RAW数据
 */
AIDataBuffer GenerateTestRAWData(size_t width, size_t height) {
    AIDataBuffer buffer;
    buffer.width = width;
    buffer.height = height;
    buffer.channels = 1;
    buffer.domain = DataDomain::RAW_DOMAIN;
    buffer.bit_depth = DataBitDepth::BIT16;
    buffer.size = width * height * sizeof(uint16_t);
    buffer.buffer_name = "test_raw_input";

    buffer.data = new uint8_t[buffer.size];
    uint16_t* raw_data = static_cast<uint16_t*>(buffer.data);

    // 生成带有梯度和噪声的测试图样
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> noise(0.0f, 10.0f);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            // 创建渐变背景
            float gradient = static_cast<float>(x) / width * 20000.0f +
                            static_cast<float>(y) / height * 30000.0f;

            // 添加一些图样
            float pattern = 0.0f;
            if ((x / 100 + y / 100) % 2 == 0) {
                pattern += 5000.0f; // 棋盘格图样
            }

            // 添加圆形区域
            float center_x = width / 2.0f;
            float center_y = height / 2.0f;
            float dist = std::sqrt((x - center_x) * (x - center_x) +
                                (y - center_y) * (y - center_y));
            if (dist < std::min(width, height) / 4.0f) {
                pattern += 10000.0f; // 中心亮圆
            }

            // 添加噪声
            float noise_val = noise(gen);

            // 计算最终值并钳位
            float final_val = gradient + pattern + noise_val + 1000.0f; // 偏置
            final_val = std::max(0.0f, std::min(65535.0f, final_val));

            raw_data[y * width + x] = static_cast<uint16_t>(final_val);
        }
    }

    return buffer;
}

/**
 * @brief 将RGB数据保存为PPM格式
 */
bool SaveRGBToPPM(const AIDataBuffer& rgb_buffer, const std::string& filename) {
    if (rgb_buffer.data == nullptr || rgb_buffer.channels != 3) {
        std::cerr << "Invalid RGB buffer for PPM save" << std::endl;
        return false;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // 写入PPM头部
    file << "P6\n" << rgb_buffer.width << " " << rgb_buffer.height << "\n255\n";

    // 写入像素数据
    float* rgb_data = static_cast<float*>(rgb_buffer.data);
    for (size_t i = 0; i < rgb_buffer.width * rgb_buffer.height * 3; ++i) {
        uint8_t pixel_val = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, rgb_data[i] * 255.0f)));
        file.write(reinterpret_cast<char*>(&pixel_val), 1);
    }

    file.close();
    return true;
}

/**
 * @brief 创建完整的AI ISP Pipeline
 */
std::shared_ptr<AIISPPipeline> CreateCompleteAISPPipeline() {
    auto pipeline = std::make_shared<AIISPPipeline>();

    // 创建主处理分支
    auto main_branch = std::make_shared<BranchProcessor>("main_branch", ProcessBranch::MAIN_BRANCH);

    // 添加主处理路径模块（根据流程图）
    auto rawfe = std::make_shared<RawFrontEnd>();
    auto nnxf = std::make_shared<NNFeatureExtractor>();
    auto rgblsc = std::make_shared<RGBLSC>();
    auto rgbdrc = std::make_shared<RGBDRC>();
    auto ccm = std::make_shared<ColorCorrectionMatrix>();
    auto gamma = std::make_shared<GammaCorrection>();

    main_branch->AddModule(rawfe);
    main_branch->AddModule(nnxf);  // 特征提取
    main_branch->AddModule(rgblsc);
    main_branch->AddModule(rgbdrc);
    main_branch->AddModule(ccm);
    main_branch->AddModule(gamma);

    // 创建AI处理分支
    auto ai_branch = std::make_shared<BranchProcessor>("ai_branch", ProcessBranch::AI_BRANCH);

    auto rgbds = std::make_shared<RGBDownsample>();
    auto ai_enhancement = std::make_shared<AIISPEnhancement>();

    ai_branch->AddModule(rgbds);
    ai_branch->AddModule(ai_enhancement);

    // 创建语义分割分支
    auto semantic_branch = std::make_shared<BranchProcessor>("semantic_branch", ProcessBranch::SEMANTIC_BRANCH);

    auto semantic_input = std::make_shared<SemanticInputProcessor>();
    auto bwarp_nn = std::make_shared<NNBasedWarp>();

    semantic_branch->AddModule(semantic_input);
    semantic_branch->AddModule(bwarp_nn);

    // 添加分支到Pipeline
    pipeline->AddBranch(main_branch);
    pipeline->AddBranch(ai_branch);
    pipeline->AddBranch(semantic_branch);

    return pipeline;
}

/**
 * @brief 打印处理结果统计
 */
void PrintResultStatistics(const AIISPResult& result) {
    std::cout << "\n=== Processing Result Statistics ===" << std::endl;
    std::cout << "Success: " << (result.success ? "Yes" : "No") << std::endl;
    std::cout << "Processing Time: " << std::fixed << std::setprecision(2)
              << result.processing_time_ms << " ms" << std::endl;
    std::cout << "Output Buffer: " << result.output_buffer.buffer_name << std::endl;
    std::cout << "Output Dimensions: " << result.output_buffer.width << "x"
              << result.output_buffer.height << std::endl;
    std::cout << "Output Channels: " << result.output_buffer.channels << std::endl;
    std::cout << "Output Domain: " << static_cast<int>(result.output_buffer.domain) << std::endl;

    if (!result.nn_features.features.empty()) {
        std::cout << "\nNeural Network Features (" << result.nn_features.feature_dim << "):" << std::endl;
        for (size_t i = 0; i < std::min(size_t(10), result.nn_features.features.size()); ++i) {
            std::cout << "  Feature[" << i << "]: " << std::fixed << std::setprecision(4)
                      << result.nn_features.features[i] << std::endl;
        }
    }

    if (!result.semantic_info.segmentation_map.empty()) {
        std::cout << "\nSemantic Information:" << std::endl;
        std::cout << "  Classes: " << result.semantic_info.num_classes << std::endl;
        std::cout << "  Dimensions: " << result.semantic_info.width << "x"
                  << result.semantic_info.height << std::endl;
    }

    if (!result.success) {
        std::cout << "\nError: " << result.error_message << std::endl;
    }

    std::cout << "====================================" << std::endl;
}

/**
 * @brief 运行基准测试
 */
void RunBenchmark(std::shared_ptr<AIISPPipeline> pipeline, const AIDataBuffer& input, int iterations) {
    std::cout << "\n=== Running Benchmark (" << iterations << " iterations) ===" << std::endl;

    std::vector<double> processing_times;
    int success_count = 0;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        AIISPResult result = pipeline->ProcessFrame(input);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (result.success) {
            processing_times.push_back(static_cast<double>(duration));
            success_count++;
        }

        // 清理输出缓冲区
        if (result.output_buffer.data != nullptr) {
            delete[] static_cast<uint8_t*>(result.output_buffer.data);
        }

        // 显示进度
        if ((i + 1) % 10 == 0) {
            std::cout << "Completed " << (i + 1) << "/" << iterations << " iterations" << std::endl;
        }
    }

    // 计算统计数据
    if (!processing_times.empty()) {
        double sum = 0.0;
        double min_time = processing_times[0];
        double max_time = processing_times[0];

        for (double time : processing_times) {
            sum += time;
            min_time = std::min(min_time, time);
            max_time = std::max(max_time, time);
        }

        double avg_time = sum / processing_times.size();
        double fps = 1000.0 / avg_time;

        std::cout << "\n=== Benchmark Results ===" << std::endl;
        std::cout << "Success Rate: " << success_count << "/" << iterations
                  << " (" << (100.0 * success_count / iterations) << "%)" << std::endl;
        std::cout << "Average Time: " << std::fixed << std::setprecision(2) << avg_time << " ms" << std::endl;
        std::cout << "Min Time: " << min_time << " ms" << std::endl;
        std::cout << "Max Time: " << max_time << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(1) << fps << " FPS" << std::endl;
        std::cout << "=========================" << std::endl;
    }
}

/**
 * @brief 主测试函数
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    AI ISP Pipeline Test Suite        " << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // 生成测试数据
        std::cout << "\n[1/5] Generating test data..." << std::endl;
        size_t test_width = 640;
        size_t test_height = 480;
        AIDataBuffer test_input = GenerateTestRAWData(test_width, test_height);
        std::cout << "Generated " << test_width << "x" << test_height
                  << " RAW test image" << std::endl;

        // 创建Pipeline
        std::cout << "\n[2/5] Creating AI ISP Pipeline..." << std::endl;
        auto pipeline = CreateCompleteAISPPipeline();
        pipeline->PrintPipelineInfo();

        // 处理单帧
        std::cout << "\n[3/5] Processing test frame..." << std::endl;
        auto process_start = std::chrono::high_resolution_clock::now();

        AIISPResult result = pipeline->ProcessFrame(test_input);

        auto process_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            process_end - process_start).count();

        std::cout << "Total processing time: " << total_duration << " ms" << std::endl;
        PrintResultStatistics(result);

        // 保存输出结果
        if (result.success && result.output_buffer.data != nullptr) {
            std::cout << "\n[4/5] Saving output results..." << std::endl;

            std::string output_filename = "ai_isp_output.ppm";
            if (SaveRGBToPPM(result.output_buffer, output_filename)) {
                std::cout << "Output saved to: " << output_filename << std::endl;
            } else {
                std::cout << "Failed to save output" << std::endl;
            }
        }

        // 运行基准测试
        std::cout << "\n[5/5] Running performance benchmark..." << std::endl;
        int benchmark_iterations = 10;
        RunBenchmark(pipeline, test_input, benchmark_iterations);

        // 显示最终性能统计
        std::cout << "\n=== Final Performance Statistics ===" << std::endl;
        auto perf_stats = pipeline->GetPerformanceStats();
        for (const auto& stat : perf_stats) {
            std::cout << stat.first << ": " << std::fixed << std::setprecision(2)
                      << stat.second;
            if (stat.first.find("fps") != std::string::npos ||
                stat.first.find("time") != std::string::npos) {
                std::cout << (stat.first.find("fps") != std::string::npos ? " FPS" : " ms");
            }
            std::cout << std::endl;
        }

        // 清理
        std::cout << "\nCleaning up..." << std::endl;
        delete[] static_cast<uint8_t*>(test_input.data);
        if (result.output_buffer.data != nullptr) {
            delete[] static_cast<uint8_t*>(result.output_buffer.data);
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "    All Tests Completed Successfully!  " << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}