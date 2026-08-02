/**
 * @file common.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-28
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 * @brief 文件功能说明（中文）
 * 本文件是 HDR-ISP 公共基础设施的实现文件，提供 ISP 系统中通用的
 * 文件读写辅助函数。主要功能包括：
 *   1. ReadFileToMem：将磁盘上的原始 RAW / 配置文件读取到内存缓冲区
 *   2. WriteMemToFile：将内存中的图像数据写回到磁盘文件
 * 这些函数是 ISP pipeline 各模块加载输入数据与保存处理结果的基础工具。
 */

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include "common/common.h"


/**
 * @brief 将文件内容读取到指定内存缓冲区
 *        （用于加载 RAW 图、配置表、查找表等）
 *
 * @param file_name 待读取的文件名（绝对或相对路径）
 * @param mem       目标内存指针，调用方需保证已分配足够空间
 * @param size      需要读取的字节数
 * @return size_t   成功返回 fread 的返回值（读取的块数，1 表示成功）；
 *                  失败返回 -1（注意：返回类型为 size_t，-1 会被转成很大的无符号值）
 */
size_t ReadFileToMem(std::string file_name, void* mem, int size)
{
    FILE* fp;

    // 以二进制只读方式打开文件，避免在 Windows 上对 \r\n 做转换
    fp = fopen(file_name.c_str(), "rb");

    if (!fp) {
        LOG(ERROR) << file_name << " open failed";
        return -1;
    }

    // 一次读取 size 字节为一个块，读取 1 块
    auto rd = fread(mem, size, 1, fp);

    fclose(fp);

    return rd;
}


/**
 * @brief 将内存缓冲区内容写入到磁盘文件
 *        （用于保存 ISP 处理后的图像结果）
 *
 * @param file_name 目标文件名（若存在会被覆盖）
 * @param mem       源内存指针
 * @param size      需要写入的字节数
 * @return size_t   成功返回 fwrite 的返回值（写入的块数）；
 *                  失败返回 -1
 */
size_t WriteMemToFile(std::string file_name, void* mem, int size)
{
    FILE* fp;

    // 以二进制写方式打开（不存在则创建，存在则清空）
    fp = fopen(file_name.c_str(), "wb+");

    if (!fp) {
        LOG(ERROR) << file_name << " open failed";
        return -1;
    }

    // 将 size 字节作为一个块写入一次
    int rd = fwrite(mem, size, 1, fp);

    fclose(fp);

    return rd;
}