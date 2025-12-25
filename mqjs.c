/*
 * Micro QuickJS REPL
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
#include "readline_tty.h"
#include "mquickjs.h"

static uint8_t *load_file(const char *filename, int *plen);
static void dump_error(JSContext *ctx);

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

static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JS_GC(ctx);
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

/* load a script */
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    const char *filename;
    JSCStringBuf buf_str;
    uint8_t *buf;
    int buf_len;
    JSValue ret;
    
    filename = JS_ToCString(ctx, argv[0], &buf_str);
    if (!filename)
        return JS_EXCEPTION;
    buf = load_file(filename, &buf_len);

    ret = JS_Eval(ctx, (const char *)buf, buf_len, filename, 0);
    free(buf);
    return ret;
}

/* timers */
typedef struct {
    BOOL allocated;
    JSGCRef func;
    int64_t timeout; /* in ms */
} JSTimer;

#define MAX_TIMERS 16

static JSTimer js_timer_list[MAX_TIMERS];

static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSTimer *th;
    int delay, i;
    JSValue *pfunc;
    
    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "not a function");
    if (JS_ToInt32(ctx, &delay, argv[1]))
        return JS_EXCEPTION;
    for(i = 0; i < MAX_TIMERS; i++) {
        th = &js_timer_list[i];
        if (!th->allocated) {
            pfunc = JS_AddGCRef(ctx, &th->func);
            *pfunc = argv[0];
            th->timeout = get_time_ms() + delay;
            th->allocated = TRUE;
            return JS_NewInt32(ctx, i);
        }
    }
    return JS_ThrowInternalError(ctx, "too many timers");
}

static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int timer_id;
    JSTimer *th;

    if (JS_ToInt32(ctx, &timer_id, argv[0]))
        return JS_EXCEPTION;
    if (timer_id >= 0 && timer_id < MAX_TIMERS) {
        th = &js_timer_list[timer_id];
        if (th->allocated) {
            JS_DeleteGCRef(ctx, &th->func);
            th->allocated = FALSE;
        }
    }
    return JS_UNDEFINED;
}

static void run_timers(JSContext *ctx)
{
    int64_t min_delay, delay, cur_time;
    BOOL has_timer;
    int i;
    JSTimer *th;
    struct timespec ts;

    for(;;) {
        min_delay = 1000;
        cur_time = get_time_ms();
        has_timer = FALSE;
        for(i = 0; i < MAX_TIMERS; i++) {
            th = &js_timer_list[i];
            if (th->allocated) {
                has_timer = TRUE;
                delay = th->timeout - cur_time;
                if (delay <= 0) {
                    JSValue ret;
                    /* the timer expired */
                    if (JS_StackCheck(ctx, 2))
                        goto fail;
                    JS_PushArg(ctx, th->func.val); /* func name */
                    JS_PushArg(ctx, JS_NULL); /* this */
                    
                    JS_DeleteGCRef(ctx, &th->func);
                    th->allocated = FALSE;
                    
                    ret = JS_Call(ctx, 0);
                    if (JS_IsException(ret)) {
                    fail:
                        dump_error(ctx);
                        exit(1);
                    }
                    min_delay = 0;
                    break;
                } else if (delay < min_delay) {
                    min_delay = delay;
                }
            }
        }
        if (!has_timer)
            break;
        if (min_delay > 0) {
            ts.tv_sec = min_delay / 1000;
            ts.tv_nsec = (min_delay % 1000) * 1000000;
            nanosleep(&ts, NULL);
        }
    }
}

#include "mqjs_stdlib.h"

#define STYLE_DEFAULT    COLOR_BRIGHT_GREEN
#define STYLE_COMMENT    COLOR_WHITE
#define STYLE_STRING     COLOR_BRIGHT_CYAN
#define STYLE_REGEX      COLOR_CYAN
#define STYLE_NUMBER     COLOR_GREEN
#define STYLE_KEYWORD    COLOR_BRIGHT_WHITE
#define STYLE_FUNCTION   COLOR_BRIGHT_YELLOW
#define STYLE_TYPE       COLOR_BRIGHT_MAGENTA
#define STYLE_IDENTIFIER COLOR_BRIGHT_GREEN
#define STYLE_ERROR      COLOR_RED
#define STYLE_RESULT     COLOR_BRIGHT_WHITE
#define STYLE_ERROR_MSG  COLOR_BRIGHT_RED

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

static int js_log_err_flag;

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, js_log_err_flag ? stderr : stdout);
}

static void dump_error(JSContext *ctx)
{
    JSValue obj;
    obj = JS_GetException(ctx);
    fprintf(stderr, "%s", term_colors[STYLE_ERROR_MSG]);
    js_log_err_flag++;
    JS_PrintValueF(ctx, obj, JS_DUMP_LONG);
    js_log_err_flag--;
    fprintf(stderr, "%s\n", term_colors[COLOR_NONE]);
}

static int eval_buf(JSContext *ctx, const char *eval_str, const char *filename, BOOL is_repl, int parse_flags)
{
    JSValue val;
    int flags;
    
    flags = parse_flags;
    if (is_repl)
        flags |= JS_EVAL_RETVAL | JS_EVAL_REPL;
    val = JS_Parse(ctx, eval_str, strlen(eval_str), filename, flags);
    if (JS_IsException(val))
        goto exception;

    val = JS_Run(ctx, val);
    if (JS_IsException(val)) {
    exception:
        dump_error(ctx);
        return 1;
    } else {
        if (is_repl) {
            printf("%s", term_colors[STYLE_RESULT]);
            JS_PrintValueF(ctx, val, JS_DUMP_LONG);
            printf("%s\n", term_colors[COLOR_NONE]);
        }
        return 0;
    }
}

static int eval_file(JSContext *ctx, const char *filename,
                     int argc, const char **argv, int parse_flags)
{
    uint8_t *buf;
    int ret, buf_len;
    JSValue val;
    
    buf = load_file(filename, &buf_len);
    if (JS_IsBytecode(buf, buf_len)) {
        if (JS_RelocateBytecode(ctx, buf, buf_len)) {
            fprintf(stderr, "Could not relocate bytecode\n");
            exit(1);
        }
        val = JS_LoadBytecode(ctx, buf);
    } else {
        val = JS_Parse(ctx, (char *)buf, buf_len, filename, parse_flags);
    }
    if (JS_IsException(val))
        goto exception;

    if (argc > 0) {
        JSValue obj, arr;
        JSGCRef arr_ref, val_ref;
        int i;
        
        JS_PUSH_VALUE(ctx, val);
        /* must be defined after JS_LoadBytecode() */
        arr = JS_NewArray(ctx, argc);
        JS_PUSH_VALUE(ctx, arr);
        for(i = 0; i < argc; i++) {
            JS_SetPropertyUint32(ctx, arr_ref.val, i,
                                 JS_NewString(ctx, argv[i]));
        }
        JS_POP_VALUE(ctx, arr);
        obj = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, obj, "scriptArgs", arr);
        JS_POP_VALUE(ctx, val);
    }
    
    
    val = JS_Run(ctx, val);
    if (JS_IsException(val)) {
    exception:
        dump_error(ctx);
        ret = 1;
    } else {
        ret = 0;
    }
    free(buf);
    return ret;
}

static void compile_file(const char *filename, const char *outfilename,
                         size_t mem_size, int dump_memory, int parse_flags, BOOL force_32bit)
{
    uint8_t *mem_buf;
    JSContext *ctx;
    char *eval_str;
    JSValue val;
    union {
        JSBytecodeHeader hdr;
#if JSW == 8
        JSBytecodeHeader32 hdr32;
#endif
    } hdr_buf;
    int hdr_len;
    const uint8_t *data_buf;
    uint32_t data_len;
    FILE *f;
    
    /* When compiling to a file, the actual content of the stdlib does
       not matter because the generated bytecode does not depend on
       it. We still need it so that the atoms for the parsing are
       defined. The JSContext must be discarded once the compilation
       is done. */
    mem_buf = malloc(mem_size);
    ctx = JS_NewContext2(mem_buf, mem_size, &js_stdlib, TRUE);
    JS_SetLogFunc(ctx, js_log_func);

    eval_str = (char *)load_file(filename, NULL);

    val = JS_Parse(ctx, eval_str, strlen(eval_str), filename, parse_flags);
    free(eval_str);
    if (JS_IsException(val)) {
        dump_error(ctx);
        return;
    }

#if JSW == 8 
    if (force_32bit) {
        if (JS_PrepareBytecode64to32(ctx, &hdr_buf.hdr32, &data_buf, &data_len, val)) {
            fprintf(stderr, "Could not convert the bytecode from 64 to 32 bits\n");
            exit(1);
        }
        hdr_len = sizeof(JSBytecodeHeader32);
    } else
#endif
    {
        JS_PrepareBytecode(ctx, &hdr_buf.hdr, &data_buf, &data_len, val);
        
        if (dump_memory)
            JS_DumpMemory(ctx, (dump_memory >= 2));
        
        /* Relocate to zero to have a deterministic
           output. JS_DumpMemory() cannot work once the heap is relocated,
           so we relocate after it. */
        JS_RelocateBytecode2(ctx, &hdr_buf.hdr, (uint8_t *)data_buf, data_len, 0, FALSE);
        hdr_len = sizeof(JSBytecodeHeader);
    }
    f = fopen(outfilename, "wb");
    if (!f) {
        perror(outfilename);
        exit(1);
    }
    fwrite(&hdr_buf, 1, hdr_len, f);
    fwrite(data_buf, 1, data_len, f);
    fclose(f);
    
    JS_FreeContext(ctx);
    free(mem_buf);
}

/* repl */

static ReadlineState readline_state;
static uint8_t readline_cmd_buf[256];
static uint8_t readline_kill_buf[256];
static char readline_history[512];

void readline_find_completion(const char *cmdline)
{
}

static BOOL is_word(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        c == '_' || c == '$';
}

static const char js_keywords[] = 
    "break|case|catch|continue|debugger|default|delete|do|"
    "else|finally|for|function|if|in|instanceof|new|"
    "return|switch|this|throw|try|typeof|while|with|"
    "class|const|enum|import|export|extends|super|"
    "implements|interface|let|package|private|protected|"
    "public|static|yield|"
    "undefined|null|true|false|Infinity|NaN|"
    "eval|arguments|"
    "await|";

static const char js_types[] = "void|var|";

static BOOL find_keyword(const char *buf, size_t buf_len, const char *dict)
{
    const char *r, *p = dict;
    while (*p != '\0') {
        r = strchr(p, '|');
        if (!r)
            break;
        if ((r - p) == buf_len && !memcmp(buf, p, buf_len))
            return TRUE;
        p = r + 1;
    }
    return FALSE;
}

/* return the color for the character at position 'pos' and the number
   of characters of the same color */
static int term_get_color(int *plen, const char *buf, int pos, int buf_len)
{
    int c, color, pos1, len;

    c = buf[pos];
    if (c == '"' || c == '\'') {
        pos1 = pos + 1;
        for(;;) {
            if (buf[pos1] == '\0' || buf[pos1] == c)
                break;
            if (buf[pos1] == '\\' && buf[pos1 + 1] != '\0')
                pos1 += 2;
            else
                pos1++;
        }
        if (buf[pos1] != '\0')
            pos1++;
        len = pos1 - pos;
        color = STYLE_STRING;
    } else if (c == '/' && buf[pos + 1] == '*') {
        pos1 = pos + 2;
        while (buf[pos1] != '\0' &&
               !(buf[pos1] == '*' && buf[pos1 + 1] == '/')) {
            pos1++;
        }
        if (buf[pos1] != '\0')
            pos1 += 2;
        len = pos1 - pos;
        color = STYLE_COMMENT;
    } else if ((c >= '0' && c <= '9') || c == '.') {
        pos1 = pos + 1;
        while (is_word(buf[pos1]))
            pos1++;
        len = pos1 - pos;
        color = STYLE_NUMBER;
    } else if (is_word(c)) {
        pos1 = pos + 1;
        while (is_word(buf[pos1]))
            pos1++;
        len = pos1 - pos;
        if (find_keyword(buf + pos, len, js_keywords)) {
            color = STYLE_KEYWORD;
        } else {
            while (buf[pos1] == ' ')
                pos1++;
            if (buf[pos1] == '(') {
                color = STYLE_FUNCTION;
            } else {
                if (find_keyword(buf + pos, len, js_types)) {
                    color = STYLE_TYPE;
                } else {
                    color = STYLE_IDENTIFIER;
                }
            }
        }
    } else {
        color = STYLE_DEFAULT;
        len = 1;
    }
    *plen = len;
    return color;
}

static int js_interrupt_handler(JSContext *ctx, void *opaque)
{
    return readline_is_interrupted();
}

static void repl_run(JSContext *ctx)
{
    ReadlineState *s = &readline_state;
    const char *cmd;

    s->term_width = readline_tty_init();
    s->term_cmd_buf = readline_cmd_buf;
    s->term_kill_buf = readline_kill_buf;
    s->term_cmd_buf_size = sizeof(readline_cmd_buf);
    s->term_history = readline_history;
    s->term_history_buf_size = sizeof(readline_history);
    s->get_color = term_get_color;

    JS_SetInterruptHandler(ctx, js_interrupt_handler);

    for(;;) {
        cmd = readline_tty(&readline_state, "mqjs > ", FALSE);
        if (!cmd)
            break;
        eval_buf(ctx, cmd, "<cmdline>", TRUE, 0);
        run_timers(ctx);
    }
}

static void help(void)
{
    printf("MicroQuickJS" "\n"
           "usage: mqjs [options] [file [args]]\n"
           "-h  --help         list options\n"
           "-e  --eval EXPR    evaluate EXPR\n"
           "-i  --interactive  go to interactive mode\n"
           "-I  --include file include an additional file\n"
           "-d  --dump         dump the memory usage stats\n"
           "    --memory-limit n       limit the memory usage to 'n' bytes\n"
           "--no-column        no column number in debug information\n"
           "-o FILE            save the bytecode to FILE\n"
           "-m32               force 32 bit bytecode output (use with -o)\n");
    exit(1);
}

int main(int argc, const char **argv)
{
    int optind;
    size_t mem_size;
    int dump_memory = 0;
    int interactive = 0;
    const char *expr = NULL;
    const char *out_filename = NULL;
    const char *include_list[32];
    int include_count = 0;
    uint8_t *mem_buf;
    JSContext *ctx;
    int i, parse_flags;
    BOOL force_32bit;
    
    mem_size = 16 << 20;
    dump_memory = 0;
    parse_flags = 0;
    force_32bit = FALSE;
    
    /* cannot use getopt because we want to pass the command line to
       the script */
    optind = 1;
    while (optind < argc && *argv[optind] == '-') {
        const char *arg = argv[optind] + 1;
        const char *longopt = "";
        /* a single - is not an option, it also stops argument scanning */
        if (!*arg)
            break;
        optind++;
        if (*arg == '-') {
            longopt = arg + 1;
            arg += strlen(arg);
            /* -- stops argument scanning */
            if (!*longopt)
                break;
        }
        for (; *arg || *longopt; longopt = "") {
            char opt = *arg;
            if (opt)
                arg++;
            if (opt == 'h' || opt == '?' || !strcmp(longopt, "help")) {
                help();
                continue;
            }
            if (opt == 'e' || !strcmp(longopt, "eval")) {
                if (*arg) {
                    expr = arg;
                    break;
                }
                if (optind < argc) {
                    expr = argv[optind++];
                    break;
                }
                fprintf(stderr, "missing expression for -e\n");
                exit(2);
            }
            if (!strcmp(longopt, "memory-limit")) {
                char *p;
                double count;
                if (optind >= argc) {
                    fprintf(stderr, "expecting memory limit");
                    exit(1);
                }
                count = strtod(argv[optind++], &p);
                switch (tolower((unsigned char)*p)) {
                case 'g':
                    count *= 1024;
                    /* fall thru */
                case 'm':
                    count *= 1024;
                    /* fall thru */
                case 'k':
                    count *= 1024;
                    /* fall thru */
                default:
                    mem_size = (size_t)(count);
                    break;
                }
                continue;
            }
            if (opt == 'd' || !strcmp(longopt, "dump")) {
                dump_memory++;
                continue;
            }
            if (opt == 'i' || !strcmp(longopt, "interactive")) {
                interactive++;
                continue;
            }
            if (opt == 'o') {
                if (*arg) {
                    out_filename = arg;
                    break;
                }
                if (optind < argc) {
                    out_filename = argv[optind++];
                    break;
                }
                fprintf(stderr, "missing filename for -o\n");
                exit(2);
            }
            if (opt == 'I' || !strcmp(longopt, "include")) {
                if (optind >= argc) {
                    fprintf(stderr, "expecting filename");
                    exit(1);
                }
                if (include_count >= countof(include_list)) {
                    fprintf(stderr, "too many included files");
                    exit(1);
                }
                include_list[include_count++] = argv[optind++];
                continue;
            }
            if (!strcmp(longopt, "no-column")) {
                parse_flags |= JS_EVAL_STRIP_COL;
                continue;
            }
            if (opt == 'm' && !strcmp(arg, "32")) {
                /* XXX: using a long option is not consistent here */
                force_32bit = TRUE;
                arg += strlen(arg);
                continue;
            }
            if (opt) {
                fprintf(stderr, "qjs: unknown option '-%c'\n", opt);
            } else {
                fprintf(stderr, "qjs: unknown option '--%s'\n", longopt);
            }
            help();
        }
    }

    if (out_filename) {
        if (optind >= argc) {
            fprintf(stderr, "expecting input filename\n");
            exit(1);
        }
        compile_file(argv[optind], out_filename, mem_size, dump_memory,
                     parse_flags, force_32bit);
    } else {
        mem_buf = malloc(mem_size);
        ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);
        JS_SetLogFunc(ctx, js_log_func);
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            JS_SetRandomSeed(ctx, ((uint64_t)tv.tv_sec << 32) ^ tv.tv_usec);
        }

        for(i = 0; i < include_count; i++) {
            if (eval_file(ctx, include_list[i], 0, NULL, parse_flags))
                goto fail;
        }
        
        if (expr) {
            if (eval_buf(ctx, expr, "<cmdline>", FALSE, parse_flags | JS_EVAL_REPL))
                goto fail;
        } else if (optind >= argc) {
            interactive = 1;
        } else {
            if (eval_file(ctx, argv[optind], argc - optind, argv + optind,
                          parse_flags))
                goto fail;
        }
        
        if (interactive) {
            repl_run(ctx);
        } else {
            run_timers(ctx);
        }
        
        if (dump_memory)
            JS_DumpMemory(ctx, (dump_memory >= 2));
        
        JS_FreeContext(ctx);
        free(mem_buf);
    }
    return 0;
 fail:
    JS_FreeContext(ctx);
    free(mem_buf);
    return 1;
}
