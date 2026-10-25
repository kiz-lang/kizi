/**
 * @file kiz.hpp
 * @brief 全局公共定义（停机信号/原生函数错误/调试输出宏）
 * @author azhz1107cat
 * @date 2026-10-25
 */
#pragma once
#include <iostream>
#include <string>
#include "repl/color.hpp"

#include "version.hpp"

// 调试模式开关
#define IN_DEBUG

#undef IN_DEBUG

#ifdef IN_DEBUG
#define DEBUG_OUTPUT(msg) \
    do { \
        std::cout << Color::BRIGHT_YELLOW \
        << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " | " \
        << "msg: " << msg << Color::RESET << std::endl; \
    } while(0)

#else
#define DEBUG_OUTPUT(msg) ((void)0)
#endif

#include <exception>
#include <string>

class KizStopRunningSignal final : public std::runtime_error {
public:
    KizStopRunningSignal() noexcept
        : std::runtime_error("") {}

    explicit KizStopRunningSignal(const std::string& msg) noexcept
        : std::runtime_error(msg) {}
};

// kiz层异常被handle_throw重定向到更外层处理帧后，用于穿透中间C++调用栈
// (原生foreach/map等循环)回到拥有该处理帧的执行循环
class KizUnwindSignal final : public std::runtime_error {
public:
    KizUnwindSignal() noexcept
        : std::runtime_error("") {}
};

class NativeFuncError final : public std::runtime_error {
public:
    std::string name;
    std::string msg;
    explicit NativeFuncError(std::string  name_, std::string  msg_) noexcept
        : name(std::move(name_)), msg(msg_), std::runtime_error(msg_) {}
};