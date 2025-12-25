/*
 * Micro QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MQUICKJS_H
#define MQUICKJS_H

#include <inttypes.h>

#if defined(__GNUC__) || defined(__clang__)
#define __js_printf_like(f, a)   __attribute__((format(printf, f, a)))
#else
#define __js_printf_like(a, b)
#endif

#if INTPTR_MAX >= INT64_MAX
#define JS_PTR64 /* pointers are 64 bit wide instead of 32 bit wide */
#endif

typedef struct JSContext JSContext;

#ifdef JS_PTR64
typedef uint64_t JSWord;
typedef uint64_t JSValue;
#define JSW  8
#define JSValue_PRI  PRIo64
#define JS_USE_SHORT_FLOAT
#else
typedef uint32_t JSWord;
typedef uint32_t JSValue;
#define JSW  4
#define JSValue_PRI  PRIo32
#endif

#define JS_BOOL int

enum {
    JS_TAG_INT         = 0, /* 31 bit integer (1 bit) */
    JS_TAG_PTR         = 1, /* pointer (2 bits) */
    JS_TAG_SPECIAL     = 3, /* other special values (2 bits) */
    JS_TAG_BOOL        = JS_TAG_SPECIAL | (0 << 2), /* (5 bits) */
    JS_TAG_NULL        = JS_TAG_SPECIAL | (1 << 2), /* (5 bits) */
    JS_TAG_UNDEFINED   = JS_TAG_SPECIAL | (2 << 2), /* (5 bits) */
    JS_TAG_EXCEPTION   = JS_TAG_SPECIAL | (3 << 2), /* (5 bits) */
    JS_TAG_SHORT_FUNC  = JS_TAG_SPECIAL | (4 << 2), /* (5 bits) */
    JS_TAG_UNINITIALIZED = JS_TAG_SPECIAL | (5 << 2), /* (5 bits) */
    JS_TAG_STRING_CHAR  = JS_TAG_SPECIAL | (6 << 2), /* (5 bits) */
    JS_TAG_CATCH_OFFSET = JS_TAG_SPECIAL | (7 << 2), /* (5 bits) */
#ifdef JS_USE_SHORT_FLOAT
    JS_TAG_SHORT_FLOAT  = 5,  /* 3 bits */
#endif
};

#define JS_TAG_SPECIAL_BITS 5

#define JS_VALUE_GET_INT(v) ((int)(v) >> 1)
#define JS_VALUE_GET_SPECIAL_VALUE(v) ((int)(v) >> JS_TAG_SPECIAL_BITS)
#define JS_VALUE_GET_SPECIAL_TAG(v) ((v) & ((1 << JS_TAG_SPECIAL_BITS) - 1))
#define JS_VALUE_MAKE_SPECIAL(tag, v) ((tag) | ((v) << JS_TAG_SPECIAL_BITS))

#define JS_NULL      JS_VALUE_MAKE_SPECIAL(JS_TAG_NULL, 0)
#define JS_UNDEFINED JS_VALUE_MAKE_SPECIAL(JS_TAG_UNDEFINED, 0)
#define JS_UNINITIALIZED JS_VALUE_MAKE_SPECIAL(JS_TAG_UNINITIALIZED, 0)
#define JS_FALSE     JS_VALUE_MAKE_SPECIAL(JS_TAG_BOOL, 0)
#define JS_TRUE      JS_VALUE_MAKE_SPECIAL(JS_TAG_BOOL, 1)

#define JS_EX_NORMAL 0 /* all exceptions except not enough memory */
#define JS_EX_CALL   1 /* specific exception to generate a tail call. The call flags are added */
#define JS_EXCEPTION JS_VALUE_MAKE_SPECIAL(JS_TAG_EXCEPTION, JS_EX_NORMAL)

typedef enum {
    JS_CLASS_OBJECT,
    JS_CLASS_ARRAY,
    JS_CLASS_C_FUNCTION,
    JS_CLASS_CLOSURE,
    JS_CLASS_NUMBER,
    JS_CLASS_BOOLEAN,
    JS_CLASS_STRING,
    JS_CLASS_DATE,
    JS_CLASS_REGEXP,

    JS_CLASS_ERROR,
    JS_CLASS_EVAL_ERROR,
    JS_CLASS_RANGE_ERROR,
    JS_CLASS_REFERENCE_ERROR,
    JS_CLASS_SYNTAX_ERROR,
    JS_CLASS_TYPE_ERROR,
    JS_CLASS_URI_ERROR,
    JS_CLASS_INTERNAL_ERROR,

    JS_CLASS_ARRAY_BUFFER,
    JS_CLASS_TYPED_ARRAY,

    JS_CLASS_UINT8C_ARRAY,
    JS_CLASS_INT8_ARRAY,
    JS_CLASS_UINT8_ARRAY,
    JS_CLASS_INT16_ARRAY,
    JS_CLASS_UINT16_ARRAY,
    JS_CLASS_INT32_ARRAY,
    JS_CLASS_UINT32_ARRAY,
    JS_CLASS_FLOAT32_ARRAY,
    JS_CLASS_FLOAT64_ARRAY,

    JS_CLASS_USER, /* user classes start from this value */
} JSObjectClassEnum;

/* predefined functions */
typedef enum {
    JS_CFUNCTION_bound,
    JS_CFUNCTION_USER, /* user functions start from this value */
} JSCFunctionEnum;

/* temporary buffer to hold C strings */
typedef struct {
    uint8_t buf[5]; 
} JSCStringBuf;

typedef struct JSGCRef {
    JSValue val;
    struct JSGCRef *prev;
} JSGCRef;

/* stack of JSGCRef */
JSValue *JS_PushGCRef(JSContext *ctx, JSGCRef *ref);
JSValue JS_PopGCRef(JSContext *ctx, JSGCRef *ref);

#define JS_PUSH_VALUE(ctx, v) do { JS_PushGCRef(ctx, &v ## _ref); v ## _ref.val = v; } while (0)
#define JS_POP_VALUE(ctx, v) v = JS_PopGCRef(ctx, &v ## _ref)

/* list of JSGCRef (they can be removed in any order, slower) */
JSValue *JS_AddGCRef(JSContext *ctx, JSGCRef *ref);
void JS_DeleteGCRef(JSContext *ctx, JSGCRef *ref);

JSValue JS_NewFloat64(JSContext *ctx, double d);
JSValue JS_NewInt32(JSContext *ctx, int32_t val);
JSValue JS_NewUint32(JSContext *ctx, uint32_t val);
JSValue JS_NewInt64(JSContext *ctx, int64_t val);

static inline JS_BOOL JS_IsInt(JSValue v)
{
    return (v & 1) == JS_TAG_INT;
}

static inline JS_BOOL JS_IsPtr(JSValue v)
{
    return (v & (JSW - 1)) == JS_TAG_PTR;
}

#ifdef JS_USE_SHORT_FLOAT
static inline JS_BOOL JS_IsShortFloat(JSValue v)
{
    return (v & (JSW - 1)) == JS_TAG_SHORT_FLOAT;
}
#endif

static inline JS_BOOL JS_IsBool(JSValue v)
{
    return JS_VALUE_GET_SPECIAL_TAG(v) == JS_TAG_BOOL;
}

static inline JS_BOOL JS_IsNull(JSValue v)
{
    return v == JS_NULL;
}

static inline JS_BOOL JS_IsUndefined(JSValue v)
{
    return v == JS_UNDEFINED;
}

static inline JS_BOOL JS_IsUninitialized(JSValue v)
{
    return v == JS_UNINITIALIZED;
}

static inline JS_BOOL JS_IsException(JSValue v)
{
    return v == JS_EXCEPTION;
}

static inline JSValue JS_NewBool(int val)
{
    return JS_VALUE_MAKE_SPECIAL(JS_TAG_BOOL, (val != 0));
}

JS_BOOL JS_IsNumber(JSContext *ctx, JSValue val);
JS_BOOL JS_IsString(JSContext *ctx, JSValue val);
JS_BOOL JS_IsError(JSContext *ctx, JSValue val);
JS_BOOL JS_IsFunction(JSContext *ctx, JSValue val);

int JS_GetClassID(JSContext *ctx, JSValue val);
void JS_SetOpaque(JSContext *ctx, JSValue val, void *opaque);
void *JS_GetOpaque(JSContext *ctx, JSValue val);

typedef JSValue JSCFunction(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
/* no JS function call be called from a C finalizer */
typedef void (*JSCFinalizer)(JSContext *ctx, void *opaque);

typedef enum JSCFunctionDefEnum {  /* XXX: should rename for namespace isolation */
    JS_CFUNC_generic,
    JS_CFUNC_generic_magic,
    JS_CFUNC_constructor,
    JS_CFUNC_constructor_magic,
    JS_CFUNC_generic_params,
    JS_CFUNC_f_f,
} JSCFunctionDefEnum;

typedef union JSCFunctionType {
    JSCFunction *generic;
    JSValue (*generic_magic)(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv, int magic);
    JSCFunction *constructor;
    JSValue (*constructor_magic)(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv, int magic);
    JSValue (*generic_params)(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv, JSValue params);
    double (*f_f)(double f);
} JSCFunctionType;

typedef struct JSCFunctionDef {
    JSCFunctionType func;
    JSValue name;
    uint8_t def_type;
    uint8_t arg_count;
    int16_t magic;
} JSCFunctionDef;

typedef struct {
    const JSWord *stdlib_table;
    const JSCFunctionDef *c_function_table;
    const JSCFinalizer *c_finalizer_table;
    uint32_t stdlib_table_len;
    uint32_t stdlib_table_align;
    uint32_t sorted_atoms_offset;
    uint32_t global_object_offset;
    uint32_t class_count;
} JSSTDLibraryDef;

typedef void JSWriteFunc(void *opaque, const void *buf, size_t buf_len);
/* return != 0 if the JS code needs to be interrupted */
typedef int JSInterruptHandler(JSContext *ctx, void *opaque);

JSContext *JS_NewContext(void *mem_start, size_t mem_size, const JSSTDLibraryDef *stdlib_def);
/* if prepare_compilation is true, the context will be used to compile
   to a binary file. JS_NewContext2() is not expected to be used in
   the embedded version */
JSContext *JS_NewContext2(void *mem_start, size_t mem_size, const JSSTDLibraryDef *stdlib_def, JS_BOOL prepare_compilation);
void JS_FreeContext(JSContext *ctx);
void JS_SetContextOpaque(JSContext *ctx, void *opaque);
void JS_SetInterruptHandler(JSContext *ctx, JSInterruptHandler *interrupt_handler);
void JS_SetRandomSeed(JSContext *ctx, uint64_t seed);
JSValue JS_GetGlobalObject(JSContext *ctx);
JSValue JS_Throw(JSContext *ctx, JSValue obj);
JSValue __js_printf_like(3, 4) JS_ThrowError(JSContext *ctx, JSObjectClassEnum error_num,
                                           const char *fmt, ...);
#define JS_ThrowTypeError(ctx, fmt, ...) JS_ThrowError(ctx, JS_CLASS_TYPE_ERROR, fmt, ##__VA_ARGS__)
#define JS_ThrowReferenceError(ctx, fmt, ...) JS_ThrowError(ctx, JS_CLASS_REFERENCE_ERROR, fmt, ##__VA_ARGS__)
#define JS_ThrowInternalError(ctx, fmt, ...) JS_ThrowError(ctx, JS_CLASS_INTERNAL_ERROR, fmt, ##__VA_ARGS__)
#define JS_ThrowRangeError(ctx, fmt, ...) JS_ThrowError(ctx, JS_CLASS_RANGE_ERROR, fmt, ##__VA_ARGS__)
#define JS_ThrowSyntaxError(ctx, fmt, ...) JS_ThrowError(ctx, JS_CLASS_SYNTAX_ERROR, fmt, ##__VA_ARGS__)
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
JSValue JS_GetPropertyStr(JSContext *ctx, JSValue this_obj, const char *str);
JSValue JS_GetPropertyUint32(JSContext *ctx, JSValue obj, uint32_t idx);
JSValue JS_SetPropertyStr(JSContext *ctx, JSValue this_obj,
                          const char *str, JSValue val);
JSValue JS_SetPropertyUint32(JSContext *ctx, JSValue this_obj,
                             uint32_t idx, JSValue val);
JSValue JS_NewObjectClassUser(JSContext *ctx, int class_id);
JSValue JS_NewObject(JSContext *ctx);
JSValue JS_NewArray(JSContext *ctx, int initial_len);
/* create a C function with an object parameter (closure) */
JSValue JS_NewCFunctionParams(JSContext *ctx, int func_idx, JSValue params);

#define JS_EVAL_RETVAL    (1 << 0) /* return the last value instead of undefined (slower code) */
#define JS_EVAL_REPL      (1 << 1) /* implicitly defined global variables in assignments */
#define JS_EVAL_STRIP_COL (1 << 2) /* strip column number debug information (save memory) */
#define JS_EVAL_JSON      (1 << 3) /* parse as JSON and return the object */
#define JS_EVAL_REGEXP    (1 << 4) /* internal use */
#define JS_EVAL_REGEXP_FLAGS_SHIFT 8  /* internal use */
JSValue JS_Parse(JSContext *ctx, const char *input, size_t input_len,
                 const char *filename, int eval_flags);
JSValue JS_Run(JSContext *ctx, JSValue val);
JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags);
void JS_GC(JSContext *ctx);
JSValue JS_NewStringLen(JSContext *ctx, const char *buf, size_t buf_len);
JSValue JS_NewString(JSContext *ctx, const char *buf);
const char *JS_ToCStringLen(JSContext *ctx, size_t *plen, JSValue val, JSCStringBuf *buf);
const char *JS_ToCString(JSContext *ctx, JSValue val, JSCStringBuf *buf);
JSValue JS_ToString(JSContext *ctx, JSValue val);
int JS_ToInt32(JSContext *ctx, int *pres, JSValue val);
int JS_ToUint32(JSContext *ctx, uint32_t *pres, JSValue val);
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValue val);
int JS_ToNumber(JSContext *ctx, double *pres, JSValue val);

JSValue JS_GetException(JSContext *ctx);
int JS_StackCheck(JSContext *ctx, uint32_t len);
void JS_PushArg(JSContext *ctx, JSValue val);
#define FRAME_CF_CTOR           (1 << 16) /* also ored with argc in
                                             C constructors */
JSValue JS_Call(JSContext *ctx, int call_flags);

#define JS_BYTECODE_MAGIC   0xacfb

typedef struct {
    uint16_t magic; /* JS_BYTECODE_MAGIC */
    uint16_t version;
    uintptr_t base_addr;
    JSValue unique_strings;
    JSValue main_func;
} JSBytecodeHeader;

/* only used on the host when compiling to file */
void JS_PrepareBytecode(JSContext *ctx,
                        JSBytecodeHeader *hdr,
                        const uint8_t **pdata_buf, uint32_t *pdata_len,
                        JSValue eval_code);
/* only used on the host when compiling to file */
int JS_RelocateBytecode2(JSContext *ctx, JSBytecodeHeader *hdr,
                         uint8_t *buf, uint32_t buf_len,
                         uintptr_t new_base_addr, JS_BOOL update_atoms);
#if JSW == 8
typedef struct {
    uint16_t magic; /* JS_BYTECODE_MAGIC */
    uint16_t version;
    uint32_t base_addr;
    uint32_t unique_strings;
    uint32_t main_func;
} JSBytecodeHeader32;

/* only used on the host when compiling to file. A 32 bit bytecode is generated on a 64 bit host. */
int JS_PrepareBytecode64to32(JSContext *ctx,
                             JSBytecodeHeader32 *hdr,
                             const uint8_t **pdata_buf, uint32_t *pdata_len,
                             JSValue eval_code);
#endif

JS_BOOL JS_IsBytecode(const uint8_t *buf, size_t buf_len);
/* Relocate the bytecode in 'buf' so that it can be executed
   later. Return 0 if OK, != 0 if error */
int JS_RelocateBytecode(JSContext *ctx,
                        uint8_t *buf, uint32_t buf_len);
/* Load the precompiled bytecode from 'buf'. 'buf' must be allocated
   as long as the JSContext exists. Use JS_Run() to execute
   it. warning: the bytecode is not checked so it should come from a
   trusted source. */
JSValue JS_LoadBytecode(JSContext *ctx, const uint8_t *buf);

/* debug functions */
void JS_SetLogFunc(JSContext *ctx, JSWriteFunc *write_func);
void JS_PrintValue(JSContext *ctx, JSValue val);
#define JS_DUMP_LONG      (1 << 0) /* display object/array content */
#define JS_DUMP_NOQUOTE   (1 << 1) /* strings: no quote for identifiers */
/* for low level dumps: don't dump special properties and use specific
   quotes to distinguish string chars, unique strings and normal
   strings */
#define JS_DUMP_RAW       (1 << 2)
void JS_PrintValueF(JSContext *ctx, JSValue val, int flags);
void JS_DumpValueF(JSContext *ctx, const char *str,
                   JSValue val, int flags);
void JS_DumpValue(JSContext *ctx, const char *str,
                  JSValue val);
void JS_DumpMemory(JSContext *ctx, JS_BOOL is_long);

#endif /* MQUICKJS_H */
