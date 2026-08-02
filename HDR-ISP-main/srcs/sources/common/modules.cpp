/**
 * @file modules.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-28
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 * @brief ISP 模块注册中心实现（中文说明）
 *
 * 本文件实现 ISP 模块的注册与查询机制。所有 ISP 算法模块
 * （如 BLC、Demosaic、CCM、Gamma、LTM 等）在初始化时通过
 * RegisterIspModule 把自身注册到全局 map 中，pipeline 在构建时
 * 通过模块名查找对应的执行函数指针。
 *
 * 设计要点：
 *   - 使用静态 std::map 作为全局注册表，键为模块名（字符串），
 *     值为 IspModule 结构（含运行函数、输入/输出位宽与域类型）
 *   - 该机制实现了"插件式"管线：用户可按名字自由组合 ISP 流水线
 */

#include "modules/modules.h"
#include <map>

// 全局 ISP 模块注册表：模块名 -> IspModule 描述符
// 静态变量只在当前编译单元可见，避免命名冲突
static std::map<std::string, IspModule> s_isp_mode_map;

/**
 * @brief 注册一个 ISP 模块到全局注册表
 *
 * @param mod 待注册的模块（包含 name、run_function、in_type、out_type 等）
 * @return int  0 成功；-1 模块名为空
 *
 * 调用时机：通常在各模块的 RegisterXxxMod() 中调用，
 *           例如 RegisterBlcMod() 会构造 IspModule 并调用本函数。
 * 若同名模块已存在，会被覆盖（更新）。
 */
int RegisterIspModule(IspModule mod)
{
    if (mod.name.empty()) {
        return -1;
    }
    s_isp_mode_map[mod.name] = mod;

    return 0;
}

/**
 * @brief 根据模块名查询 ISP 模块描述符
 *
 * @param name 模块名（如 "blc"、"demosaic"）
 * @param mod  输出参数，查找到的模块信息会被拷贝到这里
 * @return int  0 找到；-1 未找到
 *
 * Pipeline 在 MakePipe 中通过本函数把字符串名解析为可执行的模块。
 */
int GetIspModuleFromName(std::string name, IspModule& mod)
{
    if (s_isp_mode_map.count(name) > 0) {
		mod = s_isp_mode_map[name];
        return 0;
    }
    return -1;
}

/**
 * @brief 打印当前注册表中所有 ISP 模块的信息
 *        用于调试与查看可用模块列表。
 *
 * 输出格式：模块名 | 输入位宽 | 输出位宽
 * 采用逆序遍历（rbegin/rend），使后注册的模块先显示，
 * 通常更接近用户自定义的模块顺序。
 *
 * @return int 固定返回 0
 */
int ShowAllIspModules()
{
    LOG(INFO) << "============= default pipeline mods show start ==============";
    // 逆序遍历注册表，便于把后注册（通常是用户自定义）的模块显示在前
    for (auto iter = s_isp_mode_map.rbegin(); iter != s_isp_mode_map.rend(); ++iter) {
		LOG(INFO) << "isp module-> [" << iter->first << "\t] | in bits [" \
                 << std::to_string((int)iter->second.in_type) << "] | out bits [" << std::to_string((int)iter->second.out_type) << "]";
    }
    LOG(INFO) << "============= default pipeline mods show end ==============";
    return 0;
}