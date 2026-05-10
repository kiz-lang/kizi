#include "include/builtin_functions.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>

#include "../../src/models/models.hpp"
#include "../depends/u8str.hpp"

namespace builtin {

model::Object* print(model::Object* self, const model::List* args) {
    dep::UTF8String text;
    for (auto arg : args->val) {
        text += dep::UTF8String(kiz::Vm::obj_to_str(arg)) + " ";
    }
    std::cout << text << std::endl;
    return model::load_nil();
}

model::Object* input(model::Object* self, const model::List* args) {
    if (! args->val.empty()) {
        const auto prompt_obj = get_one_arg(args);
        std::cout << model::cast_to_str(prompt_obj)->val;
    }
    std::string result;
    std::getline(std::cin, result);
    return new model::String(result);
}

model::Object* ischild(model::Object* self, const model::List* args) {
    kiz::Vm::assert_argc(2, args);

    const auto a = args->val[0];
    const auto b = args->val[1];
    return check_based_object(a, b);

}

model::Object* help(model::Object* self, const model::List* args) {
    const std::string text = R"(
The kiz help

Built-in Functions:
===========================
    print(...)
    input(prompt="")
    ischild(obj, for_check_obj)
    create(parent_obj=Object)
    breakpoint()
    help()
    repeat(list, count)
    range(start, step, end)
    cmd(command)
    now()
    type_of(obj)
    setattr(obj, attr_name, value)
    getattr(current_only=False, obj, attr_name, default_value)
    hasattr(current_only=False, obj, attr_name)
    delattr(obj, attr_name)
    get_refc(obj)

Built-in Objects:
===========================
    Object
    Int
    Dec
    Str
    List
    Dict
    Bool
    Func
    NFunc
    Error
    Module
    __CodeObject
    __Nil
)";
    std::cout << text;
    return model::load_nil();
}

model::Object* breakpoint(model::Object* self, const model::List* args) {
    size_t i = 0;
    for (auto& frame: kiz::Vm::call_stack) {
        std::cout << "Frame [" << i << "] " << frame->name << "\n";
        std::cout << "=================================" << "\n";
        std::cout << "Owner: " << kiz::Vm::obj_to_debug_str(frame->owner) << "\n";
        std::cout << "Pc: " << frame->pc << "\n";

        size_t j = 1;
        std::cout << "Locals: " << "\n";
        for (const auto& n : frame->code_object->var_names) {
            auto l = kiz::Vm::op_stack[frame->bp + j - 1];
            std::cout << n << " = " << kiz::Vm::obj_to_debug_str(l);
            if (j<frame->code_object->var_names.size()) std::cout << ", ";
            ++j;
        }

        std::cout << "\n";
        std::cout << "VarNames: ";
        j = 1;
        for (const auto& n: frame->code_object->var_names) {
            std::cout << n;
            if (j<frame->code_object->var_names.size()) std::cout << ", ";
            ++j;
        }

        std::cout << "\n";
        std::cout << "AttrNames: ";
        j = 1;
        for (const auto& n: frame->code_object->attr_names) {
            std::cout << n;
            if (j<frame->code_object->attr_names.size()) std::cout << ", ";
            ++j;
        }

        std::cout << "\n";
        std::cout << "FreeNames: ";
        j = 1;
        for (const auto& n: frame->code_object->free_names) {
            std::cout << n;
            if (j<frame->code_object->free_names.size()) std::cout << ", ";
            ++j;
        }

        std::cout << "\n\n";

        ++i;
    }

    std::cout << "\n";
    std::cout << "Consts: ";
    size_t j = 1;
    for (const auto c: kiz::Vm::const_pool) {
        std::cout << kiz::Vm::obj_to_str(c);
        if (j<kiz::Vm::const_pool.size()) std::cout << ", ";
        ++j;
    }

    std::cout << "continue to run? (Y/[N])";
    std::string input;
    std::getline(std::cin, input);
    std::cout.flush();
    if (input == "Y") {
        return model::load_nil();
    }
    throw KizStopRunningSignal();
}

model::Object* cmd(model::Object* self, const model::List* args) {
    auto arg_vector = args->val;
    if (arg_vector.empty()) {
        return model::load_nil();
    }
    auto instruction = arg_vector[0];
    std::system(model::cast_to_str(instruction)->val.c_str());
    return model::load_nil();
}

model::Object* now(model::Object* self, const model::List* args) {
    auto now =
        std::chrono::high_resolution_clock::now()
        .time_since_epoch();
    int64_t time = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    return new model::Int( dep::BigInt(std::to_string(time)) );
}

model::Object* range(model::Object* self, const model::List* args) {
    auto arg_vector = args->val;
    std::vector<model::Object*> range_vector;
    dep::BigInt start_int = 0;
    dep::BigInt step_int = 1;
    dep::BigInt end_int = 1;

    if (arg_vector.size() == 1) {
        auto end_obj = arg_vector[0];
        auto end_int_obj = model::cast_to_int(end_obj);
        end_int = end_int_obj->val;
    }
    else if (arg_vector.size() == 2) {
        auto start_obj = arg_vector[0];
        auto start_int_obj = model::cast_to_int(start_obj);
        start_int = start_int_obj->val;

        auto end_obj = arg_vector[1];
        auto end_int_obj = model::cast_to_int(end_obj);
        end_int = end_int_obj->val;
    }
    else if (arg_vector.size() == 3) {
        auto start_obj = arg_vector[0];
        auto start_int_obj = model::cast_to_int(start_obj);
        start_int = start_int_obj->val;

        auto step_obj = arg_vector[1];
        auto step_int_obj = model::cast_to_int(step_obj);
        step_int = step_int_obj->val;

        auto end_obj = arg_vector[2];
        auto end_int_obj = model::cast_to_int(end_obj);
        end_int = end_int_obj->val;
    } else kiz::Vm::assert_argc({1,2,3}, args);

    for (dep::BigInt i = start_int; i < end_int; i+=step_int) {
        auto i_obj = new model::Int(i);
        range_vector.emplace_back(i_obj);
    }
    return new model::List(range_vector);
}

model::Object* setattr(model::Object* self, const model::List* args) {
    const auto arg_vector = args->val;
    kiz::Vm::assert_argc(3, args);
    auto for_set = arg_vector[0];
    auto attr_name = arg_vector[1];
    auto value = arg_vector[2];
    for_set->attrs_insert(model::cast_to_str(attr_name)->val, value);
    return model::load_nil();
}

model::Object* getattr(model::Object* self, const model::List* args) {
    auto arg_vector = args->val;
    model::Object* obj;
    model::Object* attr_name;
    model::Object* default_value =  model::load_nil();
    if (arg_vector.size() == 2 or arg_vector.size() == 3) {
        obj = arg_vector[0];
        attr_name = arg_vector[1];
        if (arg_vector.size() == 3) {
            default_value = arg_vector[2];
        }
        try {
            return kiz::Vm::get_attr(obj, model::cast_to_str(attr_name)->val);
        } catch (...) {
            return default_value;
        }
    }
    if (arg_vector.size() == 4) {
        model::Object* current_only = arg_vector[0];
        obj = arg_vector[1];
        attr_name = arg_vector[2];
        default_value = arg_vector[3];
        if (kiz::Vm::is_true(current_only)) {
            if (const auto value =
                obj->attrs.find(model::cast_to_str(attr_name)->val)
            ) return value->value;
            return default_value;
        }

        try {
            return kiz::Vm::get_attr(obj, model::cast_to_str(attr_name)->val);
        } catch (...) {
            return default_value;
        }

    }
    kiz::Vm::assert_argc({2,3,4}, args);
}

model::Object* delattr(model::Object* self, const model::List* args) {
    kiz::Vm::assert_argc(2, args);
    auto arg_vector = args->val;

    model::Object* obj = arg_vector[0];
    model::Object* attr_name = arg_vector[1];
    obj->attrs.del(model::cast_to_str(attr_name)->val);
    return model::load_nil();
}

model::Object* hasattr(model::Object* self, const model::List* args) {
    auto arg_vector = args->val;
    model::Object* obj;
    model::Object* attr_name;
    if (arg_vector.size() == 2) {
        obj = arg_vector[0];
        attr_name = arg_vector[1];

        try {
            kiz::Vm::get_attr(obj, model::cast_to_str(attr_name)->val);
            return model::load_true();
        } catch (...) {
            return model::load_false();
        }
    }
    if (arg_vector.size() == 3) {
        model::Object* current_only = arg_vector[0];
        obj = arg_vector[1];
        attr_name = arg_vector[2];
        if (kiz::Vm::is_true(current_only)) {
            if (const auto value =
                obj->attrs.find(model::cast_to_str(attr_name)->val)
            ) return model::load_true();
            return model::load_false();
        }
        try {
            kiz::Vm::get_attr(obj, model::cast_to_str(attr_name)->val);
            return model::load_true();
        } catch (...) {
            return model::load_false();
        }
    }
    kiz::Vm::assert_argc({2,3}, args);
}

model::Object* get_refc(model::Object* self, const model::List* args) {
    const auto obj = get_one_arg(args);
    return new model::Int( obj->get_refc_() );
}

model::Object* create(model::Object* self, const model::List* args) {
    if (args->val.empty()) {
        auto o = new model::Object();

        o->attrs_insert("__parent__", model::based_obj);

        return o;
    }
    const auto obj = get_one_arg(args);
    if (obj->get_type() != model::Object::ObjectType::Object) {
        throw NativeFuncError("TypeError", "Cannot create object from a instance of a native type");
    }
    const auto new_obj = new model::Object();

    new_obj->attrs_insert("__parent__", obj);

    return new_obj;
}

model::Object* type_of_obj(model::Object* self, const model::List* args) {
    const auto for_check = get_one_arg(args);
    std::string type_str;
    switch (for_check->get_type()) {
        case model::Object::ObjectType::Bool: type_str = "Bool"; break;
        case model::Object::ObjectType::Int: type_str = "Int"; break;
        case model::Object::ObjectType::String: type_str = "Str"; break;
        case model::Object::ObjectType::Object: type_str = "Object"; break;
        case model::Object::ObjectType::Nil: type_str = "Nil"; break;
        case model::Object::ObjectType::Error: type_str = "Error"; break;
        case model::Object::ObjectType::Function: type_str = "Func"; break;
        case model::Object::ObjectType::List: type_str = "List"; break;
        case model::Object::ObjectType::Dictionary: type_str = "Dict"; break;
        case model::Object::ObjectType::Decimal: type_str = "Decimal"; break;
        case model::Object::ObjectType::CodeObject: type_str = "__CodeObject"; break;
        case model::Object::ObjectType::NativeFunction: type_str = "NFunc"; break;
        case model::Object::ObjectType::Module: type_str = "Module"; break;
        default: type_str = "<Unknown>"; break;
    }
    return new model::String(type_str);
}

model::Object* debug_str(model::Object* self, const model::List* args) {
    auto for_check = get_one_arg(args);
    auto debug_str = kiz::Vm::obj_to_debug_str(for_check);
    return new model::String(debug_str);
}

model::Object* attr(model::Object* self, const model::List* args) {
    auto obj = get_one_arg(args);
    std::vector<std::pair<dep::BigInt, std::pair<model::Object*, model::Object*>>> elem_list;
    for (auto& [name, obj]: obj->attrs.to_vector()) {
        obj->make_ref();
        elem_list.emplace_back(dep::hash_string(name),
            std::pair {new model::String(name), obj}
        );
    }
    return new model::Dictionary(dep::Dict(elem_list));
}

model::Object* sleep(model::Object* self, const model::List* args) {
    auto time = model::cast_to_int(get_one_arg(args)) ->val.to_unsigned_long_long();
    std::this_thread::sleep_for(std::chrono::milliseconds(time));
    return model::load_nil();
}

model::Object* open(model::Object* self, const model::List* args) {
    kiz::Vm::assert_argc(2, args);
    auto path = cast_to_str(args->val[0])->val;
    auto mode = cast_to_str(args->val[1])->val;

    auto real_path = std::filesystem::absolute(kiz::Vm::get_current_file_path().parent_path() / path);

    std::ios_base::openmode open_mode = std::ios_base::in | std::ios_base::out; // 默认不设置 binary
    // 根据 mode 设置具体标志（此处 mode 不包含 'b'，即默认文本模式）
    if (mode == "r") {
        open_mode = std::ios_base::in;
        if (!std::filesystem::is_regular_file(real_path)) {
            throw NativeFuncError("PathError", "File not found: " + real_path.string());
        }
    } else if (mode == "w") {
        open_mode = std::ios_base::out | std::ios_base::trunc;
    } else if (mode == "a") {
        open_mode = std::ios_base::out | std::ios_base::app;
    } else if (mode == "r+") {
        open_mode = std::ios_base::in | std::ios_base::out;
        if (!std::filesystem::is_regular_file(real_path)) {
            throw NativeFuncError("PathError", "File not found: " + real_path.string());
        }
    } else if (mode == "w+") {
        open_mode = std::ios_base::in | std::ios_base::out | std::ios_base::trunc;
    } else {
        throw NativeFuncError("ModeError", "Invalid file mode: " + mode);
    }

    auto file_stream = new std::fstream();
    file_stream->open(real_path.string(), open_mode);

    if (!file_stream->is_open()) {
        delete file_stream;
        throw NativeFuncError("FileOpenError", "Failed to open file: " + real_path.string());
    }

    auto fh_obj = new model::FileHandle();
    fh_obj->attrs_insert("__parent__", model::based_file_handle);
    fh_obj->attrs_insert("mode", new model::String(mode));
    fh_obj->attrs_insert("path", new model::String(real_path.string()));
    fh_obj->file_handle = file_stream; // 核心：存储文件句柄

    return fh_obj;
}


model::Object* assert_(model::Object* self, const model::List* args) {
    auto args_vec = args->val;
    std::string msg = "...";
    if (args_vec.size() == 1) {
        if (kiz::Vm::is_true(args_vec[0])) {
            return model::load_nil();
        }
    }
    else if (args_vec.size() == 2) {
        if (kiz::Vm::is_true(args_vec[0])) {
            return model::load_nil();
        }
        msg = model::cast_to_str(args_vec[1]) ->val;
    } else kiz::Vm::assert_argc({1,2}, args);

    throw NativeFuncError("Assert", msg);
}

model::Object* panic(model::Object* self, const model::List* args) {
    std::string msg = model::cast_to_str(args->val[0]) ->val;
    std::cout << Color::BRIGHT_RED << "A Panic! : " << Color::RESET << msg << std::endl;
    exit(3);
}

model::Object* repeat(model::Object* self, const model::List* args) {
    kiz::Vm::assert_argc(2, args);
    auto lst = cast_to_list(args->val[0])->val;
    auto repeat_count = cast_to_int(args->val[1])->val;

    std::vector<model::Object*> new_vec = {};

    for (dep::BigInt i = 0; i < repeat_count; i += 1) {
        for (model::Object* obj: lst) {
            new_vec.push_back(obj);
        }
    }

    return new model::List(new_vec);
}

}
