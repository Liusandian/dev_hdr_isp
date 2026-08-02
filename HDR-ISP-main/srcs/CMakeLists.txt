cmake_minimum_required (VERSION 3.5)

project(modules)

include_directories(../thirdparty/easylog)
include_directories(../thirdparty/json/nlohmann)
include_directories(../include)

#file(GLOB_RECURSE COMMON_FILES ./common/*.cpp ./common/*.h*)
#file(GLOB_RECURSE MODULES_FILES ./modules/*.cpp ./moudles/*.h*)
aux_source_directory(sources/modules/ ISP_MOD_SRCS_LISTS)
# 暂时排除image_save.cpp，因为它依赖OpenCV
# aux_source_directory(sources/common/  ISP_COM_SRCS_LISTS)
set(ISP_COM_SRCS_LISTS
    sources/common/frame.cpp
    sources/common/common.cpp
    sources/common/modules.cpp
    sources/common/pipeline.cpp
)



add_library(${PROJECT_NAME} ${ISP_MOD_SRCS_LISTS} ${ISP_COM_SRCS_LISTS})

target_link_libraries(${PROJECT_NAME} easylog)

# AI ISP 模块配置
option(BUILD_AI_ISP_MODULES "Build AI ISP modules and test program" ON)
if(BUILD_AI_ISP_MODULES)
    # AI ISP 源文件（使用引号处理路径中的空格）
    set(AI_ISP_SOURCES
        "sources/ai isp/modules/main_path_modules.cpp"
        "sources/ai isp/modules/ai_branch_modules.cpp"
        "sources/ai isp/fusion/ai_isp_fusion.cpp"
        "sources/ai isp/pipeline/ai_isp_pipeline.cpp"
    )

    # AI ISP 头文件目录
    include_directories("./include/ai isp")
    include_directories("./sources/ai isp/modules")
    include_directories("./sources/ai isp/fusion")
    include_directories("./sources/ai isp/pipeline")

    # AI ISP 库
    add_library(ai_isp_core STATIC ${AI_ISP_SOURCES})
    target_link_libraries(ai_isp_core easylog ${PROJECT_NAME})

    # AI ISP 测试程序
    add_executable(ai_isp_test_exe
        "sources/ai isp/test/simple_ai_isp_test.cpp"
    )
    target_link_libraries(ai_isp_test_exe ai_isp_core easylog ${PROJECT_NAME})

    message(STATUS "AI ISP modules: ENABLED")
    message(STATUS "  AI ISP sources: ${AI_ISP_SOURCES}")
else()
    message(STATUS "AI ISP modules: DISABLED")
endif()