/**
 * @file test_runner.hpp
 * @brief 示例测试运行器核心定义
 * 提供运行单个kiz文件与批量运行examples目录的接口
 * @author azhz1107cat
 * @date 2026-10-25
 */

#pragma once
#include <string>

/// 运行单个kiz文件（实现在main.cpp）
void run_file(const std::string& path);

/// 批量运行examples目录下的所有示例文件
void start_test();
