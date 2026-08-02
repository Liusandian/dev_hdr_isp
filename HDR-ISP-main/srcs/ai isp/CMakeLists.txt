# AI ISP Pipeline CMake Configuration
cmake_minimum_required (VERSION 3.5)

project(AI_ISP)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# AI ISP核心库
add_library(ai_isp_core
    # 模块实现
    modules/main_path_modules.cpp
    modules/ai_branch_modules.cpp

    # 融合模块
    fusion/ai_isp_fusion.cpp

    # Pipeline管理
    pipeline/ai_isp_pipeline.cpp
)

# 链接库
target_link_libraries(ai_isp_core
    easylog
    modules
)

# AI ISP测试程序
add_executable(ai_isp_test
    test/ai_isp_test.cpp
)

target_link_libraries(ai_isp_test
    ai_isp_core
    easylog
)

# 安装规则
install(TARGETS ai_isp_core ai_isp_test
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 头文件安装
install(DIRECTORY include/
    DESTINATION include/ai_isp
    FILES_MATCHING PATTERN "*.h"
)

# 可选：构建示例程序
option(BUILD_AI_ISP_EXAMPLES "Build AI ISP example programs" ON)
if(BUILD_AI_ISP_EXAMPLES)
    add_subdirectory(examples)
endif()

# 打印配置信息
message(STATUS "AI ISP Pipeline Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Examples: ${BUILD_AI_ISP_EXAMPLES}")