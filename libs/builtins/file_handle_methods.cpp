/**
 * @file file_handle_methods.cpp
 * @brief FileHandle类型的方法实现（read/write/readline/close）
 * @author azhz1107cat
 * @date 2026-10-25
 */
#include "../../src/models/models.hpp"

namespace model {

Object* file_handle_flush(Object* self, const List* args) {
    kiz::Vm::assert_argc(0, args);

    auto f_obj = dynamic_cast<FileHandle*>(self);
    assert(f_obj);

    if (f_obj->is_closed) {
        throw NativeFuncError("FileError", "Cannot flush closed file handle");
    }
    if (!f_obj->file_handle || !f_obj->file_handle->good()) {
        throw NativeFuncError("FileError", "Invalid or corrupted file handle");
    }

    f_obj->file_handle->flush();

    if (f_obj->file_handle->bad()) {
        throw NativeFuncError("FileError", "Flush failed due to stream error");
    }

    return load_nil();
}

Object* file_handle_read(Object* self, const List* args) {
    kiz::Vm::assert_argc(0, args);
    auto f_obj = dynamic_cast<FileHandle*>(self);
    assert(f_obj);

    if (f_obj->is_closed) {
        throw NativeFuncError("FileError", "Cannot read from closed file handle");
    }
    if (!f_obj->file_handle || !f_obj->file_handle->good()) {
        throw NativeFuncError("FileError", "Invalid or corrupted file handle");
    }

    // 清除 EOF 等错误状态，并将读指针移至文件开头
    f_obj->file_handle->clear();
    f_obj->file_handle->seekg(0, std::ios::beg);

    // 读取整个文件内容（带大小上限，防单个read()读入GB级文件耗尽内存）
    constexpr std::streamsize max_read_bytes = 512 * 1024 * 1024;
    std::ostringstream oss;
    std::streamsize total = 0;
    char buf[65536];
    while (f_obj->file_handle->read(buf, sizeof(buf)) || f_obj->file_handle->gcount() > 0) {
        std::streamsize got = f_obj->file_handle->gcount();
        total += got;
        if (total > max_read_bytes) {
            throw NativeFuncError("FileError", "read(): file too large");
        }
        oss.write(buf, got);
        if (got < static_cast<std::streamsize>(sizeof(buf))) break;
    }

    // 检查读取是否成功（可选）
    if (f_obj->file_handle->fail() && !f_obj->file_handle->eof()) {
        throw NativeFuncError("FileError", "Read failed");
    }
    return new String(oss.str());
}

Object* file_handle_write(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    // 类型转换并校验
    auto f_obj = dynamic_cast<FileHandle*>(self);
    assert(f_obj);

    // 校验文件句柄状态
    if (f_obj->is_closed) {
        throw NativeFuncError("FileError", "Cannot write to closed file handle");
    }
    if (!f_obj->file_handle || !f_obj->file_handle->good()) {
        throw NativeFuncError("FileError", "Invalid or corrupted file handle");
    }

    // 提取要写入的字符串内容
    std::string content = kiz::Vm::obj_to_str(args->val[0]);

    // 写入内容并刷新缓冲区
    *f_obj->file_handle << content;
    f_obj->file_handle->flush();

    // 写入后检查流状态：磁盘满/只读句柄等静默失败必须显式报错
    if (f_obj->file_handle->bad()) {
        throw NativeFuncError("FileError", "Write failed");
    }

    // 返回空对象（无返回值）
    return load_nil();
}

Object* file_handle_readline(Object* self, const List* args) {
    kiz::Vm::assert_argc(1, args);

    auto f_obj = dynamic_cast<FileHandle*>(self);
    assert(f_obj);

    if (f_obj->is_closed) {
        throw NativeFuncError("FileError", "Cannot read from closed file handle");
    }
    if (!f_obj->file_handle || !f_obj->file_handle->good()) {
        throw NativeFuncError("FileError", "Invalid or corrupted file handle");
    }

    // 安全转换参数
    auto lineno_obj = cast_to_int(args->val[0]);
    if (lineno_obj->val == dep::BigInt(0))
        return new String("");
    size_t lineno = lineno_obj->val.to_unsigned_long_long();

    std::string target_line;
    std::string current_line;
    int64_t current_lineno = 0;

    f_obj->file_handle->clear();
    // 顺序读优化：从上次readline结束的位置继续，避免每次从头重扫(O(n^2))
    if (f_obj->readline_cursor == lineno - 1) {
        if (std::getline(*f_obj->file_handle, current_line)) {
            ++current_lineno;
            target_line = current_line;
            f_obj->readline_cursor = lineno;
            // 若文件未结束（即该行后有内容），补回换行符
            if (f_obj->file_handle->peek() != EOF) {
                target_line += "\n";
            }
        }
    } else {
        f_obj->file_handle->seekg(0, std::ios::beg);
        f_obj->readline_cursor = 0;
        while (std::getline(*f_obj->file_handle, current_line)) {
            ++current_lineno;
            if (current_lineno == static_cast<int64_t>(lineno)) {
                target_line = current_line;
                f_obj->readline_cursor = lineno;
                // 若文件未结束（即该行后有内容），补回换行符
                if (f_obj->file_handle->peek() != EOF) {
                    target_line += "\n";
                }
                break;
            }
        }
    }

    // 去掉CRLF行尾残留的\r
    if (!target_line.empty() && target_line.back() == '\r') {
        target_line.pop_back();
    }

    return new String(target_line);
}

Object* file_handle_close(Object* self, const List* args) {
    kiz::Vm::assert_argc(0, args);

    // 类型转换并校验
    auto f_obj = dynamic_cast<FileHandle*>(self);
    assert(f_obj);

    if (f_obj->is_closed) {
        return load_nil();
    }

    // 关闭文件句柄并释放资源
    if (f_obj->file_handle) {
        f_obj->file_handle->close();
        delete f_obj->file_handle;
        f_obj->file_handle = nullptr;
    }

    // 标记为已关闭
    f_obj->is_closed = true;

    return load_nil();
}

}