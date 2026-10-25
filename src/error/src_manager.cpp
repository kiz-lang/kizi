/**
 * @file src_manager.cpp
 * @brief 源文件管理器（Source File Manager）核心
 * 管理Kiz代码源文件
 * 并给予错误报告器源文件错误代码的切片
 * @author azhz1107cat
 */

#include "src_manager.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../kiz.hpp"

namespace err {

std::unordered_map<std::string, std::string> SrcManager::opened_files{};

std::vector<std::string> SrcManager::splitlines(const std::string& input) {
    std::vector<std::string> lines;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        // 剥离CRLF行尾残留的\r，保证错误上下文展示干净
        if (!line.empty() and line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    return lines;
}

///| 从指定文件中提取指定行范围的内容（行号从1开始）
std::string SrcManager::get_slice(const std::string& src_path, const size_t lineno_start, const size_t lineno_end) {
    DEBUG_OUTPUT("get slice");
    // 先获取完整文件内容（依赖缓存机制）
    std::string file_content = get_file_by_path(src_path);
    std::vector<std::string> lines = splitlines(file_content);

    // 行号钳制到合法范围（防御上游异常位置信息）
    const size_t safe_start = lineno_start == 0 ? 1 : lineno_start;
    const size_t safe_end = lineno_end < safe_start ? safe_start : lineno_end;
    if (safe_start > lines.size() or safe_end > lines.size()) {
        // 警告属于诊断信息，走stderr不污染程序正常输出
        std::cerr <<  "[Warning] Invalid line range: start=" << safe_start
                  << ", end=" << safe_end << " (total lines: " << lines.size() << ")\n";
        return "";
    }

    std::string sliced_code;
    const size_t start_idx = safe_start - 1;
    const size_t end_idx = safe_end - 1;
    for (size_t i = start_idx; i <= end_idx; ++i) {
        sliced_code += lines[i] + '\n';
    }

    return sliced_code;
}

///| 获取文件内容（优先从缓存读取，未命中则新打开）
std::string SrcManager::get_file_by_path(std::string path) {
    const auto it = opened_files.find(path);
    if (it != opened_files.end()) {
         return it->second;
    }
    // 缓存未命中，新打开文件并加入缓存
    std::string file_content = read_file(path);
    opened_files.emplace(path, file_content);
    return file_content;
}

///| 打开Kiz文件并读取内容
std::string SrcManager::read_file(const std::string& path) {
    DEBUG_OUTPUT("read_file: " + path);
    std::ifstream kiz_file(path, std::ios::binary);
    if (!kiz_file.is_open()) {
        throw KizStopRunningSignal("Failed to open file: " + path);
    }

    kiz_file.seekg(0, std::ios::end);
    const std::streampos file_size = kiz_file.tellg();
    if (file_size < 0) {
        throw KizStopRunningSignal("Failed to get file size: " + path);
    }

    std::string file_content;
    file_content.resize(file_size);

    kiz_file.seekg(0, std::ios::beg);
    kiz_file.read(&file_content[0], file_size);

    if (!kiz_file) {
        throw KizStopRunningSignal("Failed to read file: " + path);
    }

    DEBUG_OUTPUT(file_content);
    return file_content;
}

} // namespace err