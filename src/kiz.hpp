#pragma once

//|      _    _                   _ 
//|     | | _(_)____   __ _ _ __ (_)
//|     | |/ / |_  /  / _` | '_ \| |
//|     |   <| |/ /  | (_| | |_) | |
//|     |_|\_\_/___|  \__,_| .__/|_|
//|                        |_|      

#define $NoJit
#define $NoSerialization

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
enum ReturnValueKind {
    ReturnValue_Int,
    ReturnValue_Decimal,
    ReturnValue_String,
    ReturnValue_Object
};

typedef struct ReturnValue {
    ReturnValueKind kind;
    union { 
        uint64_t case_int;
        double case_decimal;
        char* case_string;
        void* case_object;
    } data;
} ReturnValue;

typedef struct KizRunningConfig {
    bool no_std;
    bool no_import_files;
    char** import_paths;
} KizRunningConfig;

void kiz_run_txt(char* txt, KizRunningConfig config);
void kiz_run_file(char* path, KizRunningConfig config);
ReturnValue kiz_run_file_with_return(char* t, KizRunningConfig config);

#ifdef __cplusplus
}
#endif