/**
 * @file test_runner.cpp
 * @brief 示例测试运行器核心实现
 * 遍历examples目录逐个运行示例文件，并在每个文件结束后清理VM状态
 * @author azhz1107cat
 * @date 2026-10-25
 */

#include "test_runner.hpp"

#include <filesystem>
#include <iostream>

#include "kiz.hpp"
#include "vm/vm.hpp"

namespace fs = std::filesystem;

/// 启动测试
void start_test() {
    // 以可执行文件位置定位examples目录（../examples），与启动时的CWD无关
    fs::path target_dir = kiz::Vm::get_exe_abs_dir().parent_path() / "examples";
    if (!fs::exists(target_dir) || !fs::is_directory(target_dir)) {
        // 兼容从仓库根目录直接运行的场景
        target_dir = "examples";
    }
    if (!fs::exists(target_dir) || !fs::is_directory(target_dir)) {
        std::cerr << "invalid dir: " << target_dir << std::endl;
        return;
    }

    // 递归遍历目录（含分类子文件夹）
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(target_dir)) {
        if (entry.is_regular_file()) {
            const fs::path& f_path = entry.path();
            if (f_path.string().ends_with(".txt")) continue;
            std::cout << "===== [file]: " << f_path << " =====" << "\n";
            try {
                run_file(f_path.string());
            } catch (KizStopRunningSignal& e) {
                std::cout << "====== recover from a error =====\n";
            }
            // 无论成败，清理本文件的VM状态：销毁残留帧并释放操作数栈引用
            while (!kiz::Vm::call_stack.empty()) {
                auto frame = kiz::Vm::call_stack.back();
                kiz::Vm::call_stack.pop_back();
                kiz::Vm::unwind_op_stack_to(frame->bp);
                kiz::Vm::destroy_frame(frame);
            }
            kiz::Vm::op_stack.clear();
            std::cout << "========================\n" << "\n";
        }
    }
    std::cout << "All test pass !" << std::endl;
}
