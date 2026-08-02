# ISP调试图像保存功能使用说明

## 功能概述

为HDR-ISP流水线添加了调试图像保存功能，可以自动保存各个处理模块（如CCM、LTM等）的输入输出图像，方便调试和算法效果分析。

## 主要特性

- **自动保存**：在每个模块执行前后自动保存输入输出图像
- **多格式支持**：支持RAW、BGR、YUV等多种颜色域
- **多数据类型**：支持uint8、uint16、int32、float等多种数据类型
- **智能转换**：自动将不同数据类型转换为可显示的8位图像
- **伪彩色显示**：RAW数据使用伪彩色映射便于观察
- **命名规范**：文件名包含帧编号、模块名和阶段信息

## 文件结构

```
HDR-ISP-main/
├── srcs/
│   ├── include/
│   │   └── common/
│   │       ├── image_save.h          # 图像保存辅助类头文件
│   │       └── pipeline.h             # 更新的流水线头文件
│   └── sources/
│       ├── common/
│       │   ├── image_save.cpp         # 图像保存辅助类实现
│       │   └── pipeline.cpp           # 更新的流水线实现
│       └── examples/
│           ├── debug_save_example.cpp # 完整使用示例
│           └── test_debug_save.cpp    # 简单测试程序
```

## 使用方法

### 1. 基本使用

```cpp
#include "common/pipeline.h"

// 创建流水线
IspPipeline pipeline;

// 启用调试保存
pipeline.SetDebugSaveEnabled(true);
pipeline.SetDebugSavePath("./debug_output");

// 构建和运行流水线
pipeline.MakePipe(prms.pipe);
pipeline.RunPipe(&frame, &prms);
```

### 2. API说明

#### IspPipeline类新增方法

```cpp
// 启用或禁用调试图像保存
void SetDebugSaveEnabled(bool enabled);

// 设置调试图像保存路径
void SetDebugSavePath(const std::string &path);

// 重置帧计数器（用于文件命名）
void ResetFrameCount();
```

#### ImageSaver类（高级用法）

```cpp
// 保存Frame对象
static int SaveFrame(const Frame* frame, const std::string& filename,
                     DataPtrTypes data_type, ColorDomains domain);

// 保存RAW图像
static int SaveRawImage(const void* data, int width, int height,
                       DataPtrTypes data_type, const std::string& filename);

// 保存BGR图像
static int SaveBgrImage(const void* data, int width, int height,
                       DataPtrTypes data_type, const std::string& filename);

// 保存YUV图像
static int SaveYuvImage(const void* y, const void* u, const void* v,
                       int width, int height, DataPtrTypes data_type,
                       const std::string& filename);
```

## 输出文件命名规则

保存的图像文件按以下规则命名：

```
frame_<帧编号>_<模块名>_<阶段>.png
```

示例：
- `frame_0_unpack_input.png` - 第0帧，解包模块输入
- `frame_0_ccm_output.png` - 第0帧，CCM模块输出
- `frame_1_ltm_input.png` - 第1帧，LTM模块输入

## 各模块效果示例

### 1. 解包模块 (unpack)
- **输入**: MIPI打包的RAW数据
- **输出**: 标准uint16 RAW数据
- **观察要点**: 数据位宽变化，打包模式解除

### 2. 黑电平校正 (blc)
- **输入**: 原始RAW数据
- **输出**: 黑电平校正后的RAW数据
- **观察要点**: 暗部亮度提升，整体对比度改善

### 3. 去马赛克 (demosaic)
- **输入**: Bayer格式RAW图像
- **输出**: 完整RGB图像
- **观察要点**: 从单通道彩色滤光阵列重建全彩色图像

### 4. 色彩校正矩阵 (ccm)
- **输入**: 原始RGB图像
- **输出**: 色彩校正后的RGB图像
- **观察要点**: 色彩准确性改善，色偏校正

### 5. 局部色调映射 (ltm)
- **输入**: 线性或Gamma编码的图像
- **输出**: 局部对比度增强的图像
- **观察要点**: 亮部和暗部细节同时保留，局部对比度提升

### 6. Gamma校正 (rgb_gamma)
- **输入**: 线性空间图像
- **输出**: Gamma编码图像
- **观察要点**: 亮度响应曲线改变，更适合显示

### 7. 饱和度调整 (saturation)
- **输入**: 标准RGB图像
- **输出**: 饱和度调整后的RGB图像
- **观察要点**: 色彩鲜艳程度变化

### 8. 对比度调整 (contrast)
- **输入**: 原始图像
- **输出**: 对比度调整后的图像
- **观察要点**: 明暗差异增强或减弱

### 9. 锐化 (sharpen)
- **输入**: 原始图像
- **输出**: 锐化后的图像
- **观察要点**: 边缘细节增强，清晰度提升

## 编译说明

### 依赖项

- OpenCV (用于图像编码和保存)
- C++17或更高版本
- CMake (推荐)

### CMake配置

确保在CMakeLists.txt中添加：

```cmake
find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

target_link_libraries(your_target
    ${OpenCV_LIBS}
    # 其他依赖...
)
```

### 编译示例

```bash
# 编译测试程序
g++ -std=c++17 test_debug_save.cpp -o test_debug_save \
    -I../../include \
    -lopencv_core -lopencv_imgcodecs -lopencv_imgproc \
    -leasylogging++

# 运行测试
./test_debug_save
```

## 实际应用示例

### 完整的ISP处理流程

```cpp
#include "common/pipeline.h"
#include "common/frame.h"
#include "modules/modules.h"
#include "common/parse.h"

int main() {
    // 1. 解析配置
    IspPrms prms;
    ParseConfig("config.json", &prms);

    // 2. 创建流水线
    IspPipeline pipeline;
    pipeline.SetDebugSaveEnabled(true);
    pipeline.SetDebugSavePath("./module_debug");

    // 3. 构建处理流水线
    std::list<std::string> pipe = {
        "unpack", "blc", "demosaic", "ccm",
        "ltm", "rgb_gamma", "saturation"
    };
    pipeline.MakePipe(pipe);

    // 4. 处理图像
    Frame frame(prms.info);
    frame.ReadFileToFrame(prms.raw_file, 0);
    pipeline.RunPipe(&frame, &prms);

    return 0;
}
```

### 对比不同参数效果

```cpp
// 测试不同的CCM矩阵
for (int i = 0; i < ccm_variants.size(); ++i) {
    IspPrms prms = base_prms;
    prms.ccm_prms = ccm_variants[i];

    IspPipeline pipeline;
    pipeline.SetDebugSavePath("./ccm_comparison/variant_" + std::to_string(i));
    pipeline.SetDebugSaveEnabled(true);

    pipeline.MakePipe(pipe);
    pipeline.RunPipe(&frame, &prms);
}
```

## 故障排除

### 问题1: 图像保存失败

**可能原因**:
- 输出目录无写入权限
- 磁盘空间不足
- OpenCV库未正确链接

**解决方案**:
```bash
# 检查目录权限
ls -la debug_output/

# 检查磁盘空间
df -h

# 验证OpenCV链接
ldd your_program | grep opencv
```

### 问题2: 保存的图像全黑或全白

**可能原因**:
- 数据类型转换错误
- 像素值超出范围
- 归一化参数不正确

**解决方案**:
- 检查ImageInfo中的max_val设置
- 验证数据类型与实际数据匹配
- 查看日志中的警告信息

### 问题3: RAW图像显示不正确

**说明**: RAW图像使用伪彩色显示，不同颜色代表不同像素值，这是正常现象。

## 性能影响

- **内存开销**: 每个模块额外保存2张图像（输入+输出）
- **磁盘I/O**: 图像编码和写入会增加处理时间
- **建议**: 仅在调试时启用，生产环境禁用

## 扩展功能

### 自定义保存格式

修改`image_save.cpp`中的保存函数以支持其他格式：

```cpp
// 保存为JPEG格式（有损压缩）
cv::imwrite(filename + ".jpg", img, {cv::IMWRITE_JPEG_QUALITY, 90});

// 保存为TIFF格式（无损）
cv::imwrite(filename + ".tiff", img);
```

### 添加图像水印

```cpp
cv::putText(img, "Module: CCM", cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
```

## 贡献指南

欢迎提交改进建议和bug报告！

## 许可证

遵循HDR-ISP主项目的许可证。