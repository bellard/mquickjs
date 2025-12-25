/*
 * Micro QuickJS C API example
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
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>

#include "cutils.h"
#include "mquickjs.h"

#define JS_CLASS_RECTANGLE (JS_CLASS_USER + 0)
#define JS_CLASS_FILLED_RECTANGLE (JS_CLASS_USER + 1)
/* total number of classes */
#define JS_CLASS_COUNT (JS_CLASS_USER + 2)

#define JS_CFUNCTION_rectangle_closure_test (JS_CFUNCTION_USER + 0)

typedef struct {
    int x;
    int y;
} RectangleData;

typedef struct {
    RectangleData parent;
    int color;
} FilledRectangleData;

static JSValue js_rectangle_constructor(JSContext *ctx, JSValue *this_val, int argc,
                                        JSValue *argv)
{
    JSValue obj;
    RectangleData *d;

    if (!(argc & FRAME_CF_CTOR))
        return JS_ThrowTypeError(ctx, "must be called with new");
    argc &= ~FRAME_CF_CTOR;
    obj = JS_NewObjectClassUser(ctx, JS_CLASS_RECTANGLE);
    d = malloc(sizeof(*d));
    JS_SetOpaque(ctx, obj, d);
    if (JS_ToInt32(ctx, &d->x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &d->y, argv[1]))
        return JS_EXCEPTION;
    return obj;
}

static void js_rectangle_finalizer(JSContext *ctx, void *opaque)
{
    RectangleData *d = opaque;
    free(d);
}

static JSValue js_rectangle_get_x(JSContext *ctx, JSValue *this_val, int argc,
                                  JSValue *argv)
{
    RectangleData *d;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_RECTANGLE && class_id != JS_CLASS_FILLED_RECTANGLE)
        return JS_ThrowTypeError(ctx, "expecting Rectangle class");
    d = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, d->x);
}

static JSValue js_rectangle_get_y(JSContext *ctx, JSValue *this_val, int argc,
                                  JSValue *argv)
{
    RectangleData *d;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_RECTANGLE && class_id != JS_CLASS_FILLED_RECTANGLE)
        return JS_ThrowTypeError(ctx, "expecting Rectangle class");
    d = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, d->y);
}

static JSValue js_rectangle_closure_test(JSContext *ctx, JSValue *this_val, int argc,
                                         JSValue *argv, JSValue params)
{
    return params;
}

/* C closure test */
static JSValue js_rectangle_getClosure(JSContext *ctx, JSValue *this_val, int argc,
                                    JSValue *argv)
{
    return JS_NewCFunctionParams(ctx, JS_CFUNCTION_rectangle_closure_test, argv[0]);
}

/* example to call a JS function. parameters: function to call, parameter */
static JSValue js_rectangle_call(JSContext *ctx, JSValue *this_val, int argc,
                                 JSValue *argv)
{
    if (JS_StackCheck(ctx, 3))
        return JS_EXCEPTION;
    JS_PushArg(ctx, argv[1]); /* parameter */
    JS_PushArg(ctx, argv[0]); /* func name */
    JS_PushArg(ctx, JS_NULL); /* this */
    return JS_Call(ctx, 1); /* single parameter */
}

static JSValue js_filled_rectangle_constructor(JSContext *ctx, JSValue *this_val, int argc,
                                               JSValue *argv)
{
    JSGCRef obj_ref;
    JSValue *obj;
    FilledRectangleData *d;

    if (!(argc & FRAME_CF_CTOR))
        return JS_ThrowTypeError(ctx, "must be called with new");
    obj = JS_PushGCRef(ctx, &obj_ref);
    
    argc &= ~FRAME_CF_CTOR;
    *obj = JS_NewObjectClassUser(ctx, JS_CLASS_FILLED_RECTANGLE);
    d = malloc(sizeof(*d));
    JS_SetOpaque(ctx, *obj, d);
    if (JS_ToInt32(ctx, &d->parent.x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &d->parent.y, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &d->color, argv[2]))
        return JS_EXCEPTION;
    JS_PopGCRef(ctx, &obj_ref);
    return *obj;
}

static void js_filled_rectangle_finalizer(JSContext *ctx, void *opaque)
{
    FilledRectangleData *d = opaque;
    free(d);
}

static JSValue js_filled_rectangle_get_color(JSContext *ctx, JSValue *this_val, int argc,
                                             JSValue *argv)
{
    FilledRectangleData *d;
    if (JS_GetClassID(ctx, *this_val) != JS_CLASS_FILLED_RECTANGLE)
        return JS_ThrowTypeError(ctx, "expecting FilledRectangle class");
    d = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, d->color);
}

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;
    
    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            fwrite(str, 1, len, stdout);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

#if defined(__linux__) || defined(__APPLE__)
static int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}
#else
static int64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}
#endif

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, get_time_ms());
}

#include "example_stdlib.h"

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, stdout);
}

static uint8_t *load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    fread(buf, 1, buf_len, f);
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}

int main(int argc, const char **argv)
{
    size_t mem_size;
    int buf_len;
    uint8_t *mem_buf, *buf;
    JSContext *ctx;
    const char *filename;
    JSValue val;
    
    if (argc < 2) {
        printf("usage: example script.js\n");
        exit(1);
    }

    filename = argv[1];

    mem_size = 65536;
    mem_buf = malloc(mem_size);
    ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);
    JS_SetLogFunc(ctx, js_log_func);
    
    buf = load_file(filename, &buf_len);
    val = JS_Eval(ctx, (const char *)buf, buf_len, filename, 0);
    free(buf);
    if (JS_IsException(val)) {
        JSValue obj;
        obj = JS_GetException(ctx);
        JS_PrintValueF(ctx, obj, JS_DUMP_LONG);
        printf("\n");
        exit(1);
    }
    
    JS_FreeContext(ctx);
    free(mem_buf);
    return 0;
}
