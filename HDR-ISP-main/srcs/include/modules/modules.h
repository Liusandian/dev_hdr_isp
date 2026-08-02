#ifndef ISP_MODS_H
#define ISP_MODS_H

#include <list>
#include <functional>
#include "common/frame.h"
#include "common/common.h"
#include <cmath>

/**
 * @brief 流水线全局配置结构体（中文说明）
 *
 * 该结构体用于控制ISP流水线的整体行为，包括日志级别、性能统计、验证策略等。
 * 所有配置项都可以通过JSON配置文件的"pipeline_config"部分进行设置。
 */
struct PipelineConfig
{
    bool enable_performance_tracking = true;    // 启用性能统计：记录每个模块的执行耗时
    bool enable_strict_validation = true;        // 启用严格的域和位宽验证：模块间必须严格匹配
    std::string log_level = "INFO";              // 日志级别：DEBUG, INFO, WARNING, ERROR, FATAL
    bool enable_module_timing = true;            // 启用模块执行时间统计
    bool stop_on_first_error = true;             // 遇到第一个错误时停止：true立即停止，false继续执行
    bool print_pipeline_on_start = true;         // 开始时打印流水线信息：显示模块列表
    bool print_pipeline_on_end = true;           // 结束时打印流水线信息：显示执行结果
};

struct IspPrms
{
    std::string raw_file;
    std::string out_file_path;

    std::string sensor_name;
    int blc = 0;
    bool data_packed;
    std::list<std::string> pipe;
    ImageInfo info;
    DePwlPrms depwl_prm;
    CcmPrms ccm_prms;
    WbGain wb_gains;
    GammmaCurve y_gamma;
    GammmaCurve rgb_gamma;
    LtmPrms ltm_prms;
    SaturationPrms sat_prms;
    ContrastPrms contrast_prms;
    SharpenPrms sharpen_prms;
    LscPrms lsc_prms;
    DpcPrms dpc_prms;
    PipelineConfig pipeline_config; // 流水线全局配置
};

struct IspModule
{
    std::string name;

    DataPtrTypes in_type;
    DataPtrTypes out_type;
    ColorDomains in_domain;
    ColorDomains out_domain;

    std::function<int(Frame *, const IspPrms *)> run_function;
};

int RegisterIspModule(IspModule mod);
int GetIspModuleFromName(std::string, IspModule &mod);
int ShowAllIspModules();

/**
 * @brief register isp module instance
 */

void RegisterUnpackMod();
void RegisterDePwlMod();
void RegisterBlcMod();
void RegisterDemoasicMod();
void RegisterCcmMod();
void RegisterYGammaMod();
void RegisterWbGaincMod();
void RegisterLtmMod();
void RegisterRgbGammaMod();
void RegisterYuv2RgbMod();
void RegisterRgb2YuvMod();
void RegisterSaturationMod();
void RegisterContrastMod();
void RegisterSharpenMod();
void RegisterLscMod();
void RegisterDpcMod();
void RegisterDpcMod();
void RegisterCnsMod();

#endif