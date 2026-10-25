/**
 * @file error_reporter.cpp
 * @brief 错误报告器（Error Reporter）核心实现
 * kiz错误报告器
 * @author azhz1107cat
 */

#include "error_reporter.hpp"

#include "src_manager.hpp"

#include <iostream>
#include <string>

#include "../kiz.hpp"
#include "../repl/color.hpp"

namespace err {

void context_printer(
    const std::string& src_path,
    const PositionInfo& pos
) {
#ifdef __EMSCRIPTEN__
#else
	size_t src_line_start = pos.lno_start == 0 ? 1 : pos.lno_start;
	size_t src_line_end = pos.lno_end == 0 ? src_line_start : pos.lno_end;
	size_t src_col_start = pos.col_start == 0 ? 1 : pos.col_start;
	size_t src_col_end = pos.col_end == 0 ? src_col_start : pos.col_end;
	// 行号/列号异常时钳制到合法关系，防止size_t下溢构造巨型字符串
	if (src_line_end < src_line_start) src_line_end = src_line_start;
	if (src_col_end < src_col_start) src_col_end = src_col_start;

	// 单行展示上限（防止把二进制等超长"行"整行喷到终端）
	constexpr size_t max_line_display = 200;

	// 获取错误代码片段（可能多行）
	std::string error_slice = SrcManager::get_slice(src_path, src_line_start, src_line_end);
	bool is_valid_range = src_line_start >= 1 && src_line_end >= 1 && src_line_start <= src_line_end;
	if (error_slice.empty() && !is_valid_range) {
		error_slice = "[Can't slice the source file with "
		+ std::to_string(src_line_start) + "," + std::to_string(src_line_start)
		+ "," + std::to_string(src_col_start) + "," + std::to_string(src_col_end) + "]";
	}

	// 按行分割
	std::vector<std::string> lines = SrcManager::splitlines(error_slice);
	if (lines.empty()) {
		std::cout << std::endl;
		std::cout << Color::BRIGHT_BLUE << "File \"" << src_path << "\"" << Color::RESET << std::endl;
		std::cout << Color::WHITE << "<no source lines available>" << Color::RESET << std::endl;
		return;
	}
	size_t line_count = lines.size();

	std::cout << std::endl;
	std::cout << Color::BRIGHT_BLUE << "File \"" << src_path << "\"" << Color::RESET << std::endl;

	// 打印每一行，带行号前缀；超长行截断展示
	for (size_t i = 0; i < line_count; ++i) {
		size_t current_lineno = src_line_start + i;
		std::string line_prefix = std::to_string(current_lineno) + " | ";
		std::string line_text = lines[i];
		if (line_text.size() > max_line_display) {
			line_text = line_text.substr(0, max_line_display) + " ...";
		}
		std::cout << Color::WHITE << line_prefix << line_text << Color::RESET << std::endl;
	}

	// 箭头定位在最后一行下方
	// 计算最后一行的行号前缀长度
	std::string last_line_prefix = std::to_string(src_line_end) + " | ";
	size_t caret_col = src_col_start > max_line_display ? max_line_display : src_col_start; // 超长行截断后箭头钳制到展示区内
	size_t caret_offset = last_line_prefix.size() + (caret_col - 1); // 使用起始列（通常等于结束列，因为是一个点）
	// 生成箭头（长度基于列范围，同样钳制到展示区）
	size_t caret_len = src_col_end - src_col_start + 1;
	size_t caret_max = max_line_display - caret_col + 2;
	if (caret_len > caret_max) caret_len = caret_max;
	std::string caret = std::string(caret_len, '^');
	std::cout << std::string(caret_offset, ' ') << Color::BRIGHT_RED << caret << Color::RESET << std::endl;
#endif
}


void error_reporter(
    const std::string& src_path,
    const PositionInfo& pos,
    const std::string& error_name,
    const std::string& error_content
) {
#ifdef __EMSCRIPTEN__
    std::cout << error_name << ":" << error_content << Color::RESET << std::endl;
#else
    context_printer(src_path, pos);
    // 错误信息（类型加粗红 + 内容白）
    std::cout << Color::BOLD << Color::BRIGHT_RED << error_name
              << Color::RESET << Color::WHITE << " : " << error_content
              << Color::RESET << std::endl;
    std::cout << std::endl;

    throw KizStopRunningSignal();
#endif
}

}// namespace err