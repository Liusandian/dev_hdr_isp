/**
 * @file parse.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-29
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */

#include "common/common.h"
#include "modules/modules.h"
#include "json.hpp"
#include <fstream>

using json = nlohmann::json;

// 字符串分割函数：将字符串按照指定分隔符分割成多个子字符串
// 参数：str - 待分割的原始字符串，pattern - 分隔符模式
// 返回值：包含所有分割后子字符串的列表
std::list<std::string> split(const std::string &str, const std::string &pattern)
{
    std::list<std::string> res;
    if (str == "")
        return res;
    std::string strs = str + pattern;
    size_t pos = strs.find(pattern);

    while (pos != strs.npos)
    {
        std::string temp = strs.substr(0, pos);
        res.push_back(temp);

        strs = strs.substr(pos + 1, strs.size());
        pos = strs.find(pattern);
    }

    return res;
}

// ISP配置文件解析函数：从JSON格式配置文件中解析ISP处理参数
// 参数：cfg_file_path - 配置文件的路径，isp_prm - 输出参数，用于存储解析后的ISP参数
// 返回值：成功返回0，失败返回-1
int ParseIspCfgFile(const std::string cfg_file_path, IspPrms &isp_prm)
{

    std::ifstream fs(cfg_file_path);

    // 检查配置文件是否成功打开
    if (!fs.is_open())
    {
        LOG(ERROR) << cfg_file_path << " open failed";
        return -1;
    }

    json j_root;

    // 将JSON文件内容解析到json对象中
    fs >> j_root;

    try {
        // 解析基础文件路径配置
        // raw path
        isp_prm.raw_file = j_root["raw_file"];
        isp_prm.out_file_path = j_root["out_file_path"];

        // 传感器信息解析
        // sensor info
        isp_prm.sensor_name = j_root["info"]["sensor_name"];
        LOG(INFO) << "Sensor Name: " << isp_prm.sensor_name;

        // CFA（彩色滤波阵列）类型解析：根据字符串转换为枚举类型
        auto cfa_str = j_root["info"]["cfa"];
        if (cfa_str == "RGGB")
        {
            isp_prm.info.cfa = CfaTypes::RGGB;
        }
        else if (cfa_str == "BGGR")
        {
            isp_prm.info.cfa = CfaTypes::BGGR;
        }
        else if (cfa_str == "GBRG")
        {
            isp_prm.info.cfa = CfaTypes::GBRG;
        }
        else if (cfa_str == "GRBG")
        {
            isp_prm.info.cfa = CfaTypes::GRBG;
        }
        LOG(INFO) << "Sensor CFA: " << cfa_str;

        // RAW数据类型解析：根据字符串转换为对应的RAW数据位深枚举类型
        auto raw_type_str = j_root["info"]["data_type"];
        if (raw_type_str == "RAW10")
        {
            isp_prm.info.dt = RawDataTypes::RAW10;
        }
        else if (raw_type_str == "RAW12")
        {
            isp_prm.info.dt = RawDataTypes::RAW12;
        }
        else if (raw_type_str == "RAW14")
        {
            isp_prm.info.dt = RawDataTypes::RAW14;
        }
        else if (raw_type_str == "RAW16")
        {
            isp_prm.info.dt = RawDataTypes::RAW16;
        }
        LOG(INFO) << "Sensor DT: " << raw_type_str;

        // 传感器基本参数解析：包括位深、最大值、分辨率、MIPI打包格式等
        isp_prm.info.bpp = static_cast<int>(j_root["info"]["bpp"]);
        int max_bit = int(j_root["info"]["max_bit"]);
        // 计算最大像素值：(2^max_bit) - 1
        isp_prm.info.max_val = (1 << max_bit) - 1;
        isp_prm.info.width = static_cast<int>(j_root["info"]["width"]);
        isp_prm.info.height = static_cast<int>(j_root["info"]["height"]);
        isp_prm.info.mipi_packed = static_cast<int>(j_root["info"]["mipi_packed"]);
        LOG(INFO) << "Sensor Resolution: " << isp_prm.info.width << "*" << isp_prm.info.height;

        // ISP处理管道解析：将管道字符串按"|"分割成多个处理步骤
        std::string pipeline = j_root["pipe"];
        isp_prm.pipe = std::move(split(pipeline, "|"));
        // 黑电平校正（BLC）参数解析
        // blc
        isp_prm.blc = j_root["blc"];

        // 白平衡增益解析：分别解析D65和D50光源下的增益值
        // wbgain
        auto d65_gains = j_root["wb_gain"]["d65_gain"];
        // 验证增益值的数量必须为4个（对应RGGB四个通道）
        if (d65_gains.size() != 4)
        {
            LOG(ERROR) << "d65 gains size error";
            return -1;
        }
        for (int i = 0; i < d65_gains.size(); ++i)
        {
            isp_prm.wb_gains.d65_gain[i] = d65_gains[i];
            //LOG(INFO) << "d65" << isp_prm.wb_gains.d65_gain[i];
        }

        // 解析D50光源下的白平衡增益
        auto d50_gains = j_root["wb_gain"]["d50_gain"];
        if (d50_gains.size() != 4)
        {
            LOG(ERROR) << "d50 gains size error";
            return -1;
        }
        for (int i = 0; i < d50_gains.size(); ++i)
        {
            isp_prm.wb_gains.d50_gain[i] = d50_gains[i];
        }

        // 去伪影分段线性曲线（DEPWL）参数解析
        // pwl
        isp_prm.depwl_prm.pedestal = j_root["depwl"]["pedestal"];
        isp_prm.depwl_prm.pwl_nums = j_root["depwl"]["pwl_nums"];
        auto pwl_x = j_root["depwl"]["pwl_x"];
        auto pwl_y = j_root["depwl"]["pwl_y"];
        auto pwl_slope = j_root["depwl"]["slope"];
        // 验证分段线性曲线的x坐标、y坐标和斜率数量是否一致
        if ((pwl_x.size() != isp_prm.depwl_prm.pwl_nums) || (pwl_y.size() != isp_prm.depwl_prm.pwl_nums) || (pwl_slope.size() != isp_prm.depwl_prm.pwl_nums))
        {
            LOG(ERROR) << "pwl input prms error";
            return -1;
        }

        // 存储分段线性曲线的坐标点和斜率
        for (int i = 0; i < isp_prm.depwl_prm.pwl_nums; ++i)
        {
            isp_prm.depwl_prm.x_cood[i] = pwl_x[i];
            isp_prm.depwl_prm.y_cood[i] = pwl_y[i];
            isp_prm.depwl_prm.slope[i] = pwl_slope[i];
        }

        // 局部色调映射（LTM）参数解析
        // pwl
        isp_prm.ltm_prms.constrast = j_root["ltm"]["constrast"];
        isp_prm.ltm_prms.in_bits = j_root["ltm"]["in_bit"];
        isp_prm.ltm_prms.out_bits = j_root["ltm"]["out_bit"];

        // RGB Gamma校正参数解析
        isp_prm.rgb_gamma.nums = j_root["rgbgamma"]["gammalut_nums"];
        isp_prm.rgb_gamma.in_bits = j_root["rgbgamma"]["in_bit"];
        isp_prm.rgb_gamma.out_bits = j_root["rgbgamma"]["out_bit"];
        auto gamma_curve = j_root["rgbgamma"]["gammalut"];
        // 验证Gamma查找表的大小是否与配置数量匹配
        if (gamma_curve.size() != isp_prm.rgb_gamma.nums)
        {
            LOG(ERROR) << "rgb gamma input prms error";
            return -1;
        }

        // 存储RGB Gamma校正曲线数据
        for (int i = 0; i < gamma_curve.size(); ++i)
        {
            isp_prm.rgb_gamma.curve[i] = gamma_curve[i];
        }

        // 亮度（Y）Gamma校正参数解析
        isp_prm.y_gamma.nums = j_root["ygamma"]["gammalut_nums"];
        isp_prm.y_gamma.in_bits = j_root["ygamma"]["in_bit"];
        isp_prm.y_gamma.out_bits = j_root["ygamma"]["out_bit"];
        gamma_curve = j_root["ygamma"]["gammalut"];
        if (gamma_curve.size() != isp_prm.y_gamma.nums)
        {
            LOG(ERROR) << "y gamma input prms error";
            return -1;
        }

        // 存储亮度Gamma校正曲线数据
        for (int i = 0; i < gamma_curve.size(); ++i)
        {
            isp_prm.y_gamma.curve[i] = gamma_curve[i];
        }

        // 饱和度调整参数解析
        isp_prm.sat_prms.rotate_angle = j_root["saturation"]["rotate_angle"];

        // 对比度调整参数解析
        isp_prm.contrast_prms.ratio = j_root["contrast"]["ratio"];

        // 锐化处理参数解析
        isp_prm.sharpen_prms.ratio = j_root["sharpen"]["ratio"];

        // 坏点校正（DPC）参数解析
        isp_prm.dpc_prms.thres = j_root["dpc"]["thres"];

        // DPC模式解析：根据字符串选择坏点校正算法模式
        std::string dpc_mode =  j_root["dpc"]["mode"];
        if (dpc_mode == "mean") {
            isp_prm.dpc_prms.mode = DpcMode::MEAN;
        } else {
            isp_prm.dpc_prms.mode = DpcMode::GRADIENT;
        }

        // 镜头阴影校正（LSC）参数解析
        // 验证LSC网格尺寸是否符合预期配置
        if ((j_root["lsc"]["mesh_width_nums"] != kLscMeshPointHNums)
            || (j_root["lsc"]["mesh_height_nums"] != kLscMeshPointVNums)) {
            LOG(ERROR) << "lsc config prms error";
            return -1;
        }


        // 解析LSC网格数据数组
        auto lsc_data_arr = j_root["lsc"]["data"];
        if (lsc_data_arr.size() != kLscMeshPointVNums) {
            LOG(ERROR) << "lsc config lsc_data_arr error";
            return -1;
        }

        // 存储LSC网格数据到四个颜色通道（B、Gr、Gb、R）
        for (int idy = 0; idy < kLscMeshPointVNums; ++idy) {
            auto arr = lsc_data_arr[idy];
            if (arr.size() != kLscMeshPointHNums) {
                LOG(ERROR) << "lsc config lsc_data_arr error";
                return -1;
            }
            for (int idx = 0; idx < kLscMeshPointHNums; ++idx) {
                //first verison, use one prm
                // 第一版本：所有颜色通道使用相同的校正值
                isp_prm.lsc_prms.mesh_b[idy][idx] = arr[idx];
                isp_prm.lsc_prms.mesh_gr[idy][idx] = arr[idx];
                isp_prm.lsc_prms.mesh_gb[idy][idx] = arr[idx];
                isp_prm.lsc_prms.mesh_r[idy][idx] = arr[idx];
            }
        }
    } catch (std::exception& e) {
        // 捕获JSON解析过程中的异常并输出错误信息
        LOG(ERROR) << "parse failed " << e.what();
        fs.close();
        return -1;
    }

    // 关闭配置文件并返回成功状态
    fs.close();
    LOG(INFO) << "parse exit";
    return 0;
}