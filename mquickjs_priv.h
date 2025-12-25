/* microj private definitions */
#ifndef MICROJS_PRIV_H
#define MICROJS_PRIV_H

#include "mquickjs.h"
#include "libm.h"

#define JS_DUMP /* 2.6 kB */
//#define DUMP_EXEC
//#define DUMP_FUNC_BYTECODE /* dump the bytecode of each compiled function */
//#define DUMP_REOP /* dump regexp bytecode */
//#define DUMP_GC
//#define DUMP_TOKEN /* dump parsed tokens */
/* run GC before at each malloc() and modify the allocated data pointers */
//#define DEBUG_GC
#if defined(DUMP_FUNC_BYTECODE) || defined(DUMP_EXEC)
#define DUMP_BYTECODE /* include the dump_byte_code() function */
#endif

#define JS_VALUE_TO_PTR(v) (void *)((uintptr_t)(v) - 1)
#define JS_VALUE_FROM_PTR(ptr) (JSWord)((uintptr_t)(ptr) + 1)

#define JS_IS_ROM_PTR(ctx, ptr) ((uintptr_t)(ptr) < (uintptr_t)ctx || (uintptr_t)(ptr) >= (uintptr_t)ctx->stack_top)

enum {
    JS_MTAG_FREE,
    /* javascript values */
    JS_MTAG_OBJECT,
    JS_MTAG_FLOAT64,
    JS_MTAG_STRING,
    /* other special memory blocks */
    JS_MTAG_FUNCTION_BYTECODE,
    JS_MTAG_VALUE_ARRAY,
    JS_MTAG_BYTE_ARRAY,
    JS_MTAG_VARREF,

    JS_MTAG_COUNT,
};

/* JS_MTAG_BITS bits are reserved at the start of every memory block */
#define JS_MTAG_BITS 4

#define JS_MB_HEADER \
    JSWord gc_mark: 1; \
    JSWord mtag: (JS_MTAG_BITS - 1)

typedef enum {
    JS_PROP_NORMAL,
    JS_PROP_GETSET, /* value is a two element JSValueArray */
    JS_PROP_VARREF, /* value is a JSVarRef (used for global variables) */
    JS_PROP_SPECIAL, /* for the prototype and constructor properties in ROM */
} JSPropTypeEnum;

#define JS_MB_HEADER_DEF(tag) ((tag) << 1)
#define JS_VALUE_ARRAY_HEADER(size) (JS_MB_HEADER_DEF(JS_MTAG_VALUE_ARRAY) | ((size) << JS_MTAG_BITS))

#define JS_ROM_VALUE(offset) JS_VALUE_FROM_PTR(&js_stdlib_table[offset])

/* runtime helpers */
JSValue js_function_constructor(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv);
JSValue js_function_get_prototype(JSContext *ctx, JSValue *this_val,
                                  int argc, JSValue *argv);
JSValue js_function_set_prototype(JSContext *ctx, JSValue *this_val,
                                  int argc, JSValue *argv);
JSValue js_function_get_length_name(JSContext *ctx, JSValue *this_val,
                                    int argc, JSValue *argv, int is_name);
JSValue js_function_toString(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv);
JSValue js_function_call(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_function_apply(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv);
JSValue js_function_bind(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_function_bound(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv, JSValue params);

JSValue js_array_get_length(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv);
JSValue js_array_set_length(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv);

JSValue js_number_constructor(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv);
JSValue js_number_toString(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv);
JSValue js_number_toFixed(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv);
JSValue js_number_toExponential(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv);
JSValue js_number_toPrecision(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv);
JSValue js_number_parseInt(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv);
JSValue js_number_parseFloat(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv);

JSValue js_boolean_constructor(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv);

JSValue js_string_get_length(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv);
JSValue js_string_set_length(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv);
JSValue js_string_slice(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv);
JSValue js_string_substring(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv);
enum {
    magic_internalAt,
    magic_charAt,
    magic_charCodeAt,
    magic_codePointAt,
};
JSValue js_string_charAt(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv, int magic);
JSValue js_string_concat(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_string_indexOf(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv, int lastIndexOf);
JSValue js_string_match(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv);
JSValue js_string_replace(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv, int is_replaceAll);
JSValue js_string_search(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_string_split(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv);
JSValue js_string_toLowerCase(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv, int to_lower);
JSValue js_string_trim(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv, int magic);

JSValue js_object_constructor(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv);
JSValue js_object_defineProperty(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv);
JSValue js_object_getPrototypeOf(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv);
JSValue js_object_setPrototypeOf(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv);
JSValue js_object_create(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_object_keys(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv);
JSValue js_object_hasOwnProperty(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv);
JSValue js_object_toString(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv);

JSValue js_string_constructor(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv);
JSValue js_string_fromCharCode(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv, int is_fromCodePoint);

JSValue js_error_constructor(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv, int magic);
JSValue js_error_toString(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv);
JSValue js_error_get_message(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv, int magic);

JSValue js_array_constructor(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv);
JSValue js_array_push(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv, int is_unshift);
JSValue js_array_pop(JSContext *ctx, JSValue *this_val,
                     int argc, JSValue *argv);
JSValue js_array_shift(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv);
JSValue js_array_join(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv);
JSValue js_array_toString(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv);
JSValue js_array_isArray(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_array_reverse(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv);
JSValue js_array_concat(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv);
JSValue js_array_indexOf(JSContext *ctx, JSValue *this_val,
                         int argc, JSValue *argv, int is_lastIndexOf);
JSValue js_array_slice(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv);
JSValue js_array_splice(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv);
JSValue js_array_sort(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv);

#define js_special_every    0
#define js_special_some     1
#define js_special_forEach  2
#define js_special_map      3
#define js_special_filter   4

JSValue js_array_every(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv, int special);

#define js_special_reduce       0
#define js_special_reduceRight  1

JSValue js_array_reduce(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv, int special);

JSValue js_math_min_max(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv, int magic);
double js_math_sign(double a);
double js_math_fround(double a);
JSValue js_math_imul(JSContext *ctx, JSValue *this_val,
                     int argc, JSValue *argv);
JSValue js_math_clz32(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv);
JSValue js_math_atan2(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv);
JSValue js_math_pow(JSContext *ctx, JSValue *this_val,
                    int argc, JSValue *argv);
JSValue js_math_random(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv);

JSValue js_array_buffer_constructor(JSContext *ctx, JSValue *this_val,
                                    int argc, JSValue *argv);
JSValue js_array_buffer_get_byteLength(JSContext *ctx, JSValue *this_val,
                                       int argc, JSValue *argv);
JSValue js_typed_array_base_constructor(JSContext *ctx, JSValue *this_val,
                                        int argc, JSValue *argv);
JSValue js_typed_array_constructor(JSContext *ctx, JSValue *this_val,
                                   int argc, JSValue *argv, int magic);
JSValue js_typed_array_get_length(JSContext *ctx, JSValue *this_val,
                                  int argc, JSValue *argv, int magic);
JSValue js_typed_array_subarray(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv);
JSValue js_typed_array_set(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv);

JSValue js_date_constructor(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv);

JSValue js_global_eval(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv);
JSValue js_global_isNaN(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv);
JSValue js_global_isFinite(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv);

JSValue js_json_parse(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv);
JSValue js_json_stringify(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv);

JSValue js_regexp_constructor(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv);
JSValue js_regexp_get_lastIndex(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv);
JSValue js_regexp_set_lastIndex(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv);
JSValue js_regexp_get_source(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv);
JSValue js_regexp_get_flags(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv);
JSValue js_regexp_exec(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv, int is_test);

#endif /* MICROJS_PRIV_H */
