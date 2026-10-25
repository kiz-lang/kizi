/**
 * @file main.cpp
 * @brief 程序入口文件
 *
 * 负责初始化程序环境、解析命令行参数、启动repl
 * @author azhz1107cat
 * @date 2025 10-25
 */

#include "repl/repl.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winnls.h>
#endif

#include <iostream>

#include "kiz.hpp"
#include "os/include/os_lib.hpp"
#include "error/src_manager.hpp"

bool nocolor = false;

/// 提供命令行帮助信息函数
void show_help(const char* prog_name);

/// 命令行参数解析函数
void args_parser(int argc, char* argv[]);

#include "test_runner.hpp"

/// 主函数
int main(const int argc, char* argv[]) {
    args_parser(argc, argv);
    std::cout << Color::RESET << std::endl;
    return 0;
}

/// 提供命令行帮助信息函数
void show_help();

void enable_ansi_escape() {
#ifdef _WIN32
    // Windows 平台的颜色支持
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD outMode = 0;
        if (GetConsoleMode(hOut, &outMode)) {
            // 只有在成功获取控制台模式时才尝试设置
            if (!SetConsoleMode(hOut, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                // 设置失败只是警告，不影响程序运行
                std::cerr << "Warning: Console does not support ANSI color (STD_OUTPUT)" << std::endl;
                nocolor = true;
            }
        } else {
            // 获取控制台模式失败（可能在测试环境或无控制台情况）
            // 静默失败，不输出错误信息，以免影响测试
        }
    }

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD errMode = 0;
        if (GetConsoleMode(hErr, &errMode)) {
            if (!SetConsoleMode(hErr, errMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                std::cerr << "Warning: Console does not support ANSI color (STD_ERROR)" << std::endl;
                nocolor = true;
            }
        }
        // 获取模式失败时静默处理
    }

    // UTF-8 编码设置
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

#ifdef _WIN32
#include <io.h>
#define STDOUT_FILENO 1

// Windows 控制台事件处理函数
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:        // Ctrl+C
    case CTRL_BREAK_EVENT:    // Ctrl+Break
    case CTRL_CLOSE_EVENT:    // 控制台窗口关闭
    case CTRL_LOGOFF_EVENT:   // 用户注销
    case CTRL_SHUTDOWN_EVENT: // 系统关机
        // 使用 write 直接输出颜色重置（异步安全）
        _write(STDOUT_FILENO, Color::RESET.c_str(), strlen(Color::RESET.c_str()));
        // 立即终止进程，不调用任何非异步信号安全的函数
        ExitProcess(128 + (DWORD)dwCtrlType);
    default:
        return FALSE;
    }
}
#else
// POSIX 系统 (Linux, macOS)
#include <csignal>
#include <unistd.h>

extern "C" void handle_sigint(int sig) {
    // 直接使用 write 输出重置颜色（异步信号安全）
    write(STDOUT_FILENO, Color::RESET.c_str(), strlen(Color::RESET.c_str()));
    _exit(128 + sig);
}
#endif


/**
 * @brief 解析命令行参数（argc/argv）
 * @param argc 命令行参数个数（来自main函数）
 * @param argv 命令行参数数组（来自main函数）
 * @return void
 */
void args_parser(const int argc, char* argv[]) {
    // 程序名称
    enable_ansi_escape();
    // 注册平台特定的信号处理函数
#ifdef _WIN32
    // Windows: 设置控制台事件处理
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr << "Waring: Cannot blind Ctrl+C Handler" << std::endl;
        return;
    }
#else
    // POSIX: 设置 SIGINT 处理
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // 不设置 SA_RESTART，使 getline 能被中断
    sigaction(SIGINT, &sa, nullptr);
#endif

    if (nocolor) {
        Color::clear_color();
    }
    const char* prog_name = argv[0];

    // 预处理：剥离全局选项（--nocolor此前根本未被解析，会被当成文件名）
    int arg_index = 1;
    while (arg_index < argc) {
        if (std::string(argv[arg_index]) == "--nocolor") {
            nocolor = true;
            Color::clear_color();
            ++arg_index;
        } else {
            break;
        }
    }
    const int eff_argc = argc - (arg_index - 1);

    // 无参数：默认启动REPL
    if (eff_argc <= 1) {
        if (argc > 1 && std::string(argv[1]) == "run") {
            // kiz run 后没有跟路径
            std::cerr << "Error: 'run' requires a file path" << std::endl;
            show_help();
            return;
        }
        ui::Repl repl;
        repl.loop();
        return;
    }

    // 1个参数 : 处理 version/repl/help/路径
    if (eff_argc == 2) {
        const std::string cmd = argv[arg_index];
        if (cmd == "version") {
            // 显示版本
            std::cout << "kiz version : " << KIZ_VERSION << std::endl;
        } else if (cmd == "repl") {
            // 显式启动REPL
            ui::Repl repl;
            repl.loop();
        } else if (cmd == "help") {
            // 显示帮助信息
            show_help();
        } else if (cmd == "__test__") {
            // 测试
            start_test();
        } else {
            std::string path = argv[arg_index];
            run_file(path);
        }
        return;
    }

    const std::string first_cmd = argv[arg_index];

    if (first_cmd == "run") {
        if (arg_index + 1 >= argc) {
            // kiz run 后没有跟路径
            std::cerr << "Error: 'run' requires a file path" << std::endl;
            show_help();
            return;
        }
        // run命令后紧跟路径
        // 如果还有更多参数，收集路径后的所有参数
        if (arg_index + 2 < argc) {
            for (int i = arg_index + 2; i < argc; ++i) {
                os_lib::rest_argv.push_back(argv[i]);
            }
        }
        std::string path = argv[arg_index + 1];
        run_file(path);
    } else {
        // 非run命令，第一个参数是路径（如 kiz a.kiz 1 2）
        // 收集路径后的所有参数
        for (int i = arg_index + 1; i < argc; ++i) {
            os_lib::rest_argv.push_back(argv[i]);
        }
        std::string path = argv[arg_index];
        run_file(path);
    }
}

void run_file(const std::string& path) {
    // 文件读取包进try：文件不存在/不可读时给出友好错误，而不是未捕获异常terminate
    std::string content;
    try {
        content = err::SrcManager::get_file_by_path(path);
    } catch (const KizStopRunningSignal& e) {
        std::cerr << "Error: " << (e.what() && *e.what() ? e.what() : "cannot open file") << ": " << path << std::endl;
        std::exit(1);
    }
    kiz::Lexer lexer(path);
    kiz::Parser parser(path);
    kiz::IRGenerator ir_gen(path);
    kiz::Vm vm (path); // 初始化vm

    try {
        lexer.prepare(content);
        const auto tokens = lexer.tokenize();
        auto ast = parser.parse(tokens);
        const auto ir = ir_gen.gen(std::move(ast));
        auto module = kiz::IRGenerator::gen_mod(path, ir);
        kiz::Vm::set_main_module(module);
        kiz::Vm::exec_curr_code();
    } catch (KizStopRunningSignal& e) {
        // 未捕获的脚本错误必须以非零码退出（空消息代表错误已由traceback打印）
        std::exit(1);
    }
}

void show_help() {
    static const std::string text = R"(
  _      _
 | | __ (_)  ____
 | |/ / | | |_  /
 |   <  | |  / /
 |_|\_\ |_| /___|


the kiz cmd help
===========================
- repl
  repl(read exec print loop)
  Type repl to start repl
  like this
  ----------------------
  | > kiz repl      |
  ----------------------

- run
  run the kiz programming file from path
  Type run for use this function
  like this
  -----------------------
  | > kiz run demo.kiz |
  -----------------------
  or
  ----------------------
  | > kiz demo.kiz    |
  ----------------------

- version
  show the version of kiz
  Type version to see the version of kiz
  like this
  -----------------------
  | > kiz version     |
  -----------------------

- help
  show this page in order to get help
  Type help to see the help of kiz
  like this
  -----------------------
  | > kiz help        |
  -----------------------
)";
    std::cout << text << std::endl;
}
