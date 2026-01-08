/*
 * Micro QuickJS build utility
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "cutils.h"
#include "list.h"
#include "mquickjs_build.h"

static unsigned JSW = 4; // override this with -m64

typedef struct {
    char *str;
    int offset;
} AtomDef;

typedef struct {
    AtomDef *tab;
    int count;
    int size;
    int offset;
} AtomList;

typedef struct {
    char *name;
    int length;
    char *magic;
    char *cproto_name;
    char *cfunc_name;
} CFuncDef;

typedef struct {
    CFuncDef *tab;
    int count;
    int size;
} CFuncList;

typedef struct {
    struct list_head link;
    const JSClassDef *class1;
    int class_idx;
    char *finalizer_name;
    char *class_id;
} ClassDefEntry;

typedef struct {
    AtomList atom_list;
    CFuncList cfunc_list;
    int cur_offset;
    int sorted_atom_table_offset;
    int global_object_offset;
    struct list_head class_list;
} BuildContext;

static const char *atoms[] = {
#define DEF(a, b) b,
    /* keywords */
    DEF(null, "null") /* must be first */
    DEF(false, "false")
    DEF(true, "true")
    DEF(if, "if")
    DEF(else, "else")
    DEF(return, "return")
    DEF(var, "var")
    DEF(this, "this")
    DEF(delete, "delete")
    DEF(void, "void")
    DEF(typeof, "typeof")
    DEF(new, "new")
    DEF(in, "in")
    DEF(instanceof, "instanceof")
    DEF(do, "do")
    DEF(while, "while")
    DEF(for, "for")
    DEF(break, "break")
    DEF(continue, "continue")
    DEF(switch, "switch")
    DEF(case, "case")
    DEF(default, "default")
    DEF(throw, "throw")
    DEF(try, "try")
    DEF(catch, "catch")
    DEF(finally, "finally")
    DEF(function, "function")
    DEF(debugger, "debugger")
    DEF(with, "with")
    /* FutureReservedWord */
    DEF(class, "class")
    DEF(const, "const")
    DEF(enum, "enum")
    DEF(export, "export")
    DEF(extends, "extends")
    DEF(import, "import")
    DEF(super, "super")
    /* FutureReservedWords when parsing strict mode code */
    DEF(implements, "implements")
    DEF(interface, "interface")
    DEF(let, "let")
    DEF(package, "package")
    DEF(private, "private")
    DEF(protected, "protected")
    DEF(public, "public")
    DEF(static, "static")
    DEF(yield, "yield")
#undef DEF

    /* other atoms */
    "",
    "toString",
    "valueOf",
    "number",
    "object",
    "undefined",
    "string",
    "boolean",
    "<ret>",
    "<eval>",
    "eval",
    "arguments",
    "value",
    "get",
    "set",
    "prototype",
    "constructor",
    "length",
    "target",
    "of",
    "NaN",
    "Infinity",
    "-Infinity",
    "name",
    "Error",
    "__proto__",
    "index",
    "input",
};


static char *cvt_name(char *buf, size_t buf_size, const char *str)
{
    size_t i, len = strlen(str);
    assert(len < buf_size);
    if (len == 0) {
        strcpy(buf, "empty");
    } else {
        strcpy(buf, str);
        for(i = 0; i < len; i++) {
            if (buf[i] == '<' || buf[i] == '>' || buf[i] == '-')
                buf[i] = '_';
        }
    }
    return buf;
}

static BOOL is_ascii_string(const char *buf, size_t len)
{
    size_t i;
    for(i = 0; i < len; i++) {
        if ((uint8_t)buf[i] > 0x7f)
            return FALSE;
    }
    return TRUE;
}

static BOOL is_numeric_string(const char *buf, size_t len)
{
    return (!strcmp(buf, "NaN") ||
            !strcmp(buf, "Infinity") ||
            !strcmp(buf, "-Infinity"));
}

static int find_atom(AtomList *s, const char *str)
{
    int i;
    for(i = 0; i < s->count; i++) {
        if (!strcmp(str, s->tab[i].str))
            return i;
    }
    return -1;
}

static int add_atom(AtomList *s, const char *str)
{
    int i;
    AtomDef *e;
    i = find_atom(s, str);
    if (i >= 0)
        return s->tab[i].offset;
    if ((s->count + 1) > s->size) {
        s->size = max_int(s->count + 1, s->size * 3 / 2);
        s->tab = realloc(s->tab, sizeof(s->tab[0]) * s->size);
    }
    e = &s->tab[s->count++];
    e->str = strdup(str);
    e->offset = s->offset;
    s->offset += 1 + ((strlen(str) + JSW) / JSW);
    return s->count - 1;
}

static int add_cfunc(CFuncList *s, const char *name, int length, const char *magic, const char *cproto_name, const char *cfunc_name)
{
    int i;
    CFuncDef *e;

    for(i = 0; i < s->count; i++) {
        e = &s->tab[i];
        if (!strcmp(name, e->name) &&
            length == e->length &&
            !strcmp(magic, e->magic) &&
            !strcmp(cproto_name, e->cproto_name) &&
            !strcmp(cfunc_name, e->cfunc_name)) {
            return i;
        }
    }
    if ((s->count + 1) > s->size) {
        s->size = max_int(s->count + 1, s->size * 3 / 2);
        s->tab = realloc(s->tab, sizeof(s->tab[0]) * s->size);
    }
    e = &s->tab[s->count++];
    e->name = strdup(name);
    e->magic = strdup(magic);
    e->length = length;
    e->cproto_name = strdup(cproto_name);
    e->cfunc_name = strdup(cfunc_name);
    return s->count - 1;
}

static void dump_atom_defines(void)
{
    AtomList atom_list_s, *s = &atom_list_s;
    AtomDef *e;
    int i;
    char buf[256];

    memset(s, 0, sizeof(*s));

    /* add the predefined atoms (they have a corresponding define) */
    for(i = 0; i < countof(atoms); i++) {
        add_atom(s, atoms[i]);
    }

    for(i = 0; i < s->count; i++) {
        e = &s->tab[i];
        printf("#define JS_ATOM_%s %d\n",
               cvt_name(buf, sizeof(buf), e->str), e->offset);
    }
    printf("\n");
    printf("#define JS_ATOM_END %d\n", s->offset);
    printf("\n");
}

static int atom_cmp(const void *p1, const void *p2)
{
    const AtomDef *a1 = (const AtomDef *)p1;
    const AtomDef *a2 = (const AtomDef *)p2;
    return strcmp(a1->str, a2->str);
}

/* js_atom_table must be properly aligned because the property hash
   table uses the low bits of the atom pointer value */
#define ATOM_ALIGN 64

static void dump_atoms(BuildContext *ctx)
{
    AtomList *s = &ctx->atom_list;
    int i, j, k, l, len, len1, is_ascii, is_numeric;
    uint64_t v;
    const char *str;
    AtomDef *sorted_atoms;
    char buf[256];

    sorted_atoms = malloc(sizeof(sorted_atoms[0]) * s->count);
    memcpy(sorted_atoms, s->tab, sizeof(sorted_atoms[0]) * s->count);
    qsort(sorted_atoms, s->count, sizeof(sorted_atoms[0]), atom_cmp);

    printf("  /* atom_table */\n");
    for(i = 0; i < s->count; i++) {
        str = s->tab[i].str;
        len = strlen(str);
        is_ascii = is_ascii_string(str, len);
        is_numeric = is_numeric_string(str, len);
        printf("  (JS_MTAG_STRING << 1) | (1 << JS_MTAG_BITS) | (%d << (JS_MTAG_BITS + 1)) | (%d << (JS_MTAG_BITS + 2)) | (%d << (JS_MTAG_BITS + 3)), /* \"%s\" (offset=%d) */\n",
               is_ascii, is_numeric, len, str, ctx->cur_offset);
        len1 = (len + JSW) / JSW;
        for(j = 0; j < len1; j++) {
            l = min_uint32(JSW, len - j * JSW);
            v = 0;
            for(k = 0; k < l; k++)
                v |= (uint64_t)(uint8_t)str[j * JSW + k] << (k * 8);
            printf("  0x%0*" PRIx64 ",\n", JSW * 2, v);
        }
        assert(ctx->cur_offset == s->tab[i].offset);
        ctx->cur_offset += len1 + 1;
    }
    printf("\n");

    ctx->sorted_atom_table_offset = ctx->cur_offset;

    printf("  /* sorted atom table (offset=%d) */\n", ctx->cur_offset);
    printf("  JS_VALUE_ARRAY_HEADER(%d),\n", s->count);
    for(i = 0; i < s->count; i++) {
        AtomDef *e = &sorted_atoms[i];
        printf("  JS_ROM_VALUE(%d), /* %s */\n",
               e->offset, cvt_name(buf, sizeof(buf), e->str));
    }
    ctx->cur_offset += s->count + 1;
    printf("\n");

    free(sorted_atoms);
}

static int define_value(BuildContext *s, const JSPropDef *d);

static uint32_t dump_atom(BuildContext *s, const char *str, BOOL value_only)
{
    int len, idx, i, offset;

    len = strlen(str);
    for(i = 0; i < len; i++) {
        if ((uint8_t)str[i] >= 128) {
            fprintf(stderr, "unicode property names are not supported yet (%s)\n", str);
            exit(1);
        }
    }
    if (len >= 1 && (str[0] >= '0' && str[0] <= '9')) {
        fprintf(stderr, "numeric property names are not supported yet (%s)\n", str);
        exit(1);
    }
    if (len == 1) {
        if (value_only) {
            /* XXX: hardcoded */
            return ((uint8_t)str[0] << 5) | 0x1b;
        }
        printf("JS_VALUE_MAKE_SPECIAL(JS_TAG_STRING_CHAR, %d)",
               (uint8_t)str[0]);
    } else {
        idx = find_atom(&s->atom_list, str);
        if (idx < 0) {
            fprintf(stderr, "atom '%s' is undefined\n", str);
            exit(1);
        }
        offset = s->atom_list.tab[idx].offset;
        if (value_only)
            return (offset * JSW) + 1; /* correct modulo ATOM_ALIGN */
        printf("JS_ROM_VALUE(%d)", offset);
    }
    printf(" /* %s */", str);
    return 0;
}

static void dump_cfuncs(BuildContext *s)
{
    int i;
    CFuncDef *e;
    
    printf("static const JSCFunctionDef js_c_function_table[] = {\n");
    for(i = 0; i < s->cfunc_list.count; i++) {
        e = &s->cfunc_list.tab[i];
        printf("  { { .%s = %s },\n", e->cproto_name, e->cfunc_name);
        printf("    ");
        dump_atom(s, e->name, FALSE);
        printf(",\n");
        printf("    JS_CFUNC_%s, %d, %s },\n",
               e->cproto_name, e->length, e->magic);
    }
    printf("};\n\n");
}

static void dump_cfinalizers(BuildContext *s)
{
    struct list_head *el;
    ClassDefEntry *e;
    
    printf("static const JSCFinalizer js_c_finalizer_table[JS_CLASS_COUNT - JS_CLASS_USER] = {\n");
    list_for_each(el, &s->class_list) {
        e = list_entry(el, ClassDefEntry, link);
        if (e->finalizer_name &&
            strcmp(e->finalizer_name, "NULL") != 0) {
            printf("  [%s - JS_CLASS_USER] = %s,\n", e->class_id, e->finalizer_name);
        }
    }
    printf("};\n\n");
}

typedef enum {
    PROPS_KIND_GLOBAL,
    PROPS_KIND_PROTO,
    PROPS_KIND_CLASS,
    PROPS_KIND_OBJECT,
} JSPropsKindEnum;

static inline uint32_t hash_prop(BuildContext *s, const char *name)
{
    /* Compute the hash for a symbol, must be consistent with
       mquickjs.c implementation.
     */
    uint32_t prop = dump_atom(s, name, TRUE);
    return (prop / JSW) ^ (prop % JSW); /* XXX: improve */
}

static int define_props(BuildContext *s, const JSPropDef *props_def,
                        JSPropsKindEnum props_kind, const char *class_id_str)
{
    int i, *ident_tab, idx, props_ident, n_props;
    int prop_idx;
    const JSPropDef *d;
    uint32_t *prop_hash;
    BOOL is_global_object = (props_kind == PROPS_KIND_GLOBAL);
    static const JSPropDef dummy_props[] = {
        { JS_DEF_END },
    };

    if (!props_def)
        props_def = dummy_props;
    
    n_props = 0;
    for(d = props_def; d->def_type != JS_DEF_END; d++) {
        n_props++;
    }
    if (props_kind == PROPS_KIND_PROTO ||
        props_kind == PROPS_KIND_CLASS)
        n_props++;
    ident_tab = malloc(sizeof(ident_tab[0]) * n_props);

    /* define the various objects */
    for(d = props_def, i = 0; d->def_type != JS_DEF_END; d++, i++) {
        ident_tab[i] = define_value(s, d);
    }

    props_ident = -1;
    prop_hash = NULL;
    if (is_global_object) {
        props_ident = s->cur_offset;
        printf("  /* global object properties (offset=%d) */\n", props_ident);
        printf("  JS_VALUE_ARRAY_HEADER(%d),\n", 2 * n_props);
        s->cur_offset += 2 * n_props + 1;
    } else {
        int hash_size_log2;
        uint32_t hash_size, hash_mask;
        uint32_t *hash_table, h;
        
        if (n_props <= 1)
            hash_size_log2 = 0;
        else
            hash_size_log2 = (32 - clz32(n_props - 1)) - 1;
        hash_size = 1 << hash_size_log2;
        if (hash_size > ATOM_ALIGN / JSW) {
#if !defined __APPLE__
            // XXX: Cannot request data alignment larger than 64 bytes on Darwin
            fprintf(stderr, "Too many properties, consider increasing ATOM_ALIGN\n");
#endif
            hash_size = ATOM_ALIGN / JSW;
        }
        hash_mask = hash_size - 1;

        hash_table = malloc(sizeof(hash_table[0]) * hash_size);
        prop_hash = malloc(sizeof(prop_hash[0]) * n_props);
        /* build the hash table */
        for(i = 0; i < hash_size; i++)
            hash_table[i] = 0;
        prop_idx = 0;
        for(i = 0, d = props_def; i < n_props; i++, d++) {
            const char *name;
            if (d->def_type != JS_DEF_END) {
                name = d->name;
            } else {
                if (props_kind == PROPS_KIND_PROTO)
                    name = "constructor";
                else
                    name = "prototype";
            }
            h = hash_prop(s, name) & hash_mask;
            prop_hash[prop_idx] = hash_table[h];
            hash_table[h] = 2 + hash_size + 3 * prop_idx;
            prop_idx++;
        }

        props_ident = s->cur_offset;
        printf("  /* properties (offset=%d) */\n", props_ident);
        printf("  JS_VALUE_ARRAY_HEADER(%d),\n", 2 + hash_size + n_props * 3);
        printf("  %d << 1, /* n_props */\n", n_props);
        printf("  %d << 1, /* hash_mask */\n", hash_mask);
        for(i = 0; i < hash_size; i++) {
            printf("  %d << 1,\n", hash_table[i]);
        }
        s->cur_offset += hash_size + 3 + 3 * n_props;
        free(hash_table);
    }
    prop_idx = 0;
    for(d = props_def, i = 0; i < n_props; d++, i++) {
        const char *name, *prop_type;
        /* name */
        printf("  ");
        if (d->def_type != JS_DEF_END) {
            name = d->name;
        } else {
            if (props_kind == PROPS_KIND_PROTO)
                name = "constructor";
            else
                name = "prototype";
        }
        dump_atom(s, name, FALSE);
        printf(",\n");

        printf("  ");
        prop_type = "NORMAL";
        switch(d->def_type) {
        case JS_DEF_PROP_DOUBLE:
            if (ident_tab[i] >= 0)
                goto value_ptr;
            /* short int */
            printf("%d << 1,", (int32_t)d->u.f64);
            break;
        case JS_DEF_CGETSET:
            if (is_global_object) {
                fprintf(stderr, "getter/setter forbidden in global object\n");
                exit(1);
            }
            prop_type = "GETSET";
            goto value_ptr;
        case JS_DEF_CLASS:
        value_ptr:
            assert(ident_tab[i] >= 0);
            printf("JS_ROM_VALUE(%d),", ident_tab[i]);
            break;
        case JS_DEF_PROP_UNDEFINED:
            printf("JS_UNDEFINED,");
            break;
        case JS_DEF_PROP_NULL:
            printf("JS_NULL,");
            break;
        case JS_DEF_PROP_STRING:
            dump_atom(s, d->u.str, FALSE);
            printf(",");
            break;
        case JS_DEF_CFUNC:
            idx = add_cfunc(&s->cfunc_list,
                            d->name,
                            d->u.func.length,
                            d->u.func.magic,
                            d->u.func.cproto_name,
                            d->u.func.func_name);
            printf("JS_VALUE_MAKE_SPECIAL(JS_TAG_SHORT_FUNC, %d),", idx);
            break;
        case JS_DEF_END:
            if (props_kind == PROPS_KIND_PROTO) {
                /* constructor property */
                printf("(uint32_t)(-%s - 1) << 1,", class_id_str);
            } else {
                /* prototype property */
                printf("%s << 1,", class_id_str);
            }
            prop_type = "SPECIAL";
            break;
        default:
            abort();
        }
        printf("\n");
        if (!is_global_object) {
            printf("  (%d << 1) | (JS_PROP_%s << 30),\n",
                   prop_hash[prop_idx], prop_type);
        }
        prop_idx++;
    }

    free(prop_hash);
    free(ident_tab);
    return props_ident;
}

static ClassDefEntry *find_class(BuildContext *s, const JSClassDef *d)
{
    struct list_head *el;
    ClassDefEntry *e;
    
    list_for_each(el, &s->class_list) {
        e = list_entry(el, ClassDefEntry, link);
        if (e->class1 == d)
            return e;
    }
    return NULL;
}

static void free_class_entries(BuildContext *s)
{
    struct list_head *el, *el1;
    ClassDefEntry *e;
    list_for_each_safe(el, el1, &s->class_list) {
        e = list_entry(el, ClassDefEntry, link);
        free(e->class_id);
        free(e->finalizer_name);
        free(e);
    }
    init_list_head(&s->class_list);
}

static int define_class(BuildContext *s, const JSClassDef *d)
{
    int ctor_func_idx = -1, class_props_idx = -1, proto_props_idx = -1;
    int ident, parent_class_idx = -1;
    ClassDefEntry *e;

    /* check if the class is already defined */
    e = find_class(s, d);
    if (e)
        return e->class_idx;
    
    if (d->parent_class)
        parent_class_idx = define_class(s, d->parent_class);
    
    if (d->func_name) {
        ctor_func_idx = add_cfunc(&s->cfunc_list,
                                  d->name,
                                  d->length,
                                  d->class_id,
                                  d->cproto_name,
                                  d->func_name);
    }

    if (ctor_func_idx >= 0) {
        class_props_idx = define_props(s, d->class_props, PROPS_KIND_CLASS, d->class_id);
        proto_props_idx = define_props(s, d->proto_props, PROPS_KIND_PROTO, d->class_id);
    } else {
        if (d->class_props)
            class_props_idx = define_props(s, d->class_props, PROPS_KIND_OBJECT, d->class_id);
    }
    
    ident = s->cur_offset;
    printf("  /* class (offset=%d) */\n", ident);
    printf("  JS_MB_HEADER_DEF(JS_MTAG_OBJECT),\n");
    if (class_props_idx >= 0)
        printf("  JS_ROM_VALUE(%d),\n", class_props_idx);
    else
        printf("  JS_NULL,\n");
    printf("  %d,\n", ctor_func_idx);
    if (proto_props_idx >= 0)
        printf("  JS_ROM_VALUE(%d),\n", proto_props_idx);
    else
        printf("  JS_NULL,\n");
    if (parent_class_idx >= 0) {
        printf("  JS_ROM_VALUE(%d),\n", parent_class_idx);
    } else {
        printf("  JS_NULL,\n");
    }
    printf("\n");
    
    s->cur_offset += 5;

    e = malloc(sizeof(*e));
    memset(e, 0, sizeof(*e));
    e->class_idx = ident;
    e->class1 = d;
    if (ctor_func_idx >= 0) {
        e->class_id = strdup(d->class_id);
        e->finalizer_name = strdup(d->finalizer_name);
    }
    list_add_tail(&e->link, &s->class_list);
    return ident;
}

#define JS_SHORTINT_MIN (-(1 << 30))
#define JS_SHORTINT_MAX ((1 << 30) - 1)

static BOOL is_short_int(double d)
{
    return (d >= JS_SHORTINT_MIN && d <= JS_SHORTINT_MAX && (int32_t)d == d);
}

static int define_value(BuildContext *s, const JSPropDef *d)
{
    int ident;
    ident = -1;
    switch(d->def_type) {
    case JS_DEF_PROP_DOUBLE:
        {
            uint64_t v;
            if (!is_short_int(d->u.f64)) {
                ident = s->cur_offset;
                printf("  /* float64 (offset=%d) */\n", ident);
                printf("  JS_MB_HEADER_DEF(JS_MTAG_FLOAT64),\n");
                v = float64_as_uint64(d->u.f64);
                if (JSW == 8) {
                    printf("  0x%016zx,\n", (size_t)v);
                    printf("\n");
                    s->cur_offset += 2;
                } else {
                    /* XXX: little endian assumed */
                    printf("  0x%08x,\n", (uint32_t)v);
                    printf("  0x%08x,\n", (uint32_t)(v >> 32));
                    printf("\n");
                    s->cur_offset += 3;
                }
            }
        }
        break;
    case JS_DEF_CLASS:
        ident = define_class(s, d->u.class1);
        break;
    case JS_DEF_CGETSET:
        {
            int get_idx = -1, set_idx = -1;
            char buf[256];
            if (strcmp(d->u.getset.get_func_name, "NULL") != 0) { 
                snprintf(buf, sizeof(buf), "get %s", d->name);
                get_idx = add_cfunc(&s->cfunc_list,
                                    buf,
                                    0, /* length */
                                    d->u.getset.magic,
                                    d->u.getset.cproto_name,
                                    d->u.getset.get_func_name);
            }
            if (strcmp(d->u.getset.set_func_name, "NULL") != 0) { 
                snprintf(buf, sizeof(buf), "set %s", d->name);
                set_idx = add_cfunc(&s->cfunc_list,
                                    buf,
                                    1, /* length */
                                    d->u.getset.magic,
                                    d->u.getset.cproto_name,
                                    d->u.getset.set_func_name);
            }
            ident = s->cur_offset;
            printf("  /* getset (offset=%d) */\n", ident);
            printf("  JS_VALUE_ARRAY_HEADER(2),\n");
            if (get_idx >= 0)
                printf("  JS_VALUE_MAKE_SPECIAL(JS_TAG_SHORT_FUNC, %d),\n", get_idx);
            else
                printf("  JS_UNDEFINED,\n");
            if (set_idx >= 0)
                printf("  JS_VALUE_MAKE_SPECIAL(JS_TAG_SHORT_FUNC, %d),\n", set_idx);
            else
                printf("  JS_UNDEFINED,\n");
            printf("\n");
            s->cur_offset += 3;
        }
        break;
    default:
        break;
    }
    return ident;
}

static void define_atoms_props(BuildContext *s, const JSPropDef *props_def, JSPropsKindEnum props_kind);

static void define_atoms_class(BuildContext *s, const JSClassDef *d)
{
    ClassDefEntry *e;
    /* check if the class is already defined */
    e = find_class(s, d);
    if (e)
        return;
    if (d->parent_class)
        define_atoms_class(s, d->parent_class);
    if (d->func_name)
        add_atom(&s->atom_list, d->name);
    if (d->class_props)
        define_atoms_props(s, d->class_props, d->func_name ? PROPS_KIND_CLASS : PROPS_KIND_OBJECT);
    if (d->proto_props)
        define_atoms_props(s, d->proto_props, PROPS_KIND_PROTO);
}

static void define_atoms_props(BuildContext *s, const JSPropDef *props_def, JSPropsKindEnum props_kind)
{
    const JSPropDef *d;
    for(d = props_def; d->def_type != JS_DEF_END; d++) {
        add_atom(&s->atom_list, d->name);
        switch(d->def_type) {
        case JS_DEF_PROP_STRING:
            add_atom(&s->atom_list, d->u.str);
            break;
        case JS_DEF_CLASS:
            define_atoms_class(s, d->u.class1);
            break;
        case JS_DEF_CGETSET:
            {
                char buf[256];
                if (strcmp(d->u.getset.get_func_name, "NULL") != 0) { 
                    snprintf(buf, sizeof(buf), "get %s", d->name);
                    add_atom(&s->atom_list, buf);
                }
                if (strcmp(d->u.getset.set_func_name, "NULL") != 0) { 
                    snprintf(buf, sizeof(buf), "set %s", d->name);
                    add_atom(&s->atom_list, buf);
                }
            }
            break;
        default:
            break;
        }
    }
}

static int usage(const char *name)
{
    fprintf(stderr, "usage: %s {-m32 | -m64} [-a]\n", name);
    fprintf(stderr,
            "    create a ROM file for the mquickjs standard library\n"
            "--help       list options\n"
            "-m32         force generation for a 32 bit target\n"
            "-m64         force generation for a 64 bit target\n"
            "-a           generate the mquickjs_atom.h header\n"
            );
    return 1;
}

int build_atoms(const char *stdlib_name, const JSPropDef *global_obj,
                const JSPropDef *c_function_decl, int argc, char **argv)
{
    int i;
    unsigned jsw;
    BuildContext ss, *s = &ss;
    BOOL build_atom_defines = FALSE;
    
#if INTPTR_MAX >= INT64_MAX
    jsw = 8;
#else
    jsw = 4;
#endif    
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-m64")) {
            jsw = 8;
        } else if (!strcmp(argv[i], "-m32")) {
            jsw = 4;
        } else if (!strcmp(argv[i], "-a")) {
            build_atom_defines = TRUE;
        } else if (!strcmp(argv[i], "--help")) {
            return usage(argv[0]);
        } else {
            fprintf(stderr, "invalid argument '%s'\n", argv[i]);
            return usage(argv[0]);
        }
    }

    JSW = jsw;
    
    if (build_atom_defines) {
        dump_atom_defines();
        return 0;
    }
    
    memset(s, 0, sizeof(*s));
    init_list_head(&s->class_list);

    /* add the predefined atoms (they have a corresponding define) */
    for(i = 0; i < countof(atoms); i++) {
        add_atom(&s->atom_list, atoms[i]);
    }

    /* add the predefined functions */
    if (c_function_decl) {
        const JSPropDef *d;
        for(d = c_function_decl; d->def_type != JS_DEF_END; d++) {
            if (d->def_type != JS_DEF_CFUNC) {
                fprintf(stderr, "only C functions are allowed in c_function_decl[]\n");
                exit(1);
            }
            add_atom(&s->atom_list, d->name);
            add_cfunc(&s->cfunc_list,
                      d->name,
                      d->u.func.length,
                      d->u.func.magic,
                      d->u.func.cproto_name,
                      d->u.func.func_name);
        }
    }

    /* first pass to define the atoms */
    define_atoms_props(s, global_obj, PROPS_KIND_GLOBAL);
    free_class_entries(s);

    printf("/* this file is automatically generated - do not edit */\n\n");
    printf("#include \"mquickjs_priv.h\"\n\n");

    printf("/* standard library functions expected to be provided by the host application */\n");
    printf("JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n");
    printf("JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);\n\n");
    
    printf("static const uint%u_t __attribute((aligned(%d))) js_stdlib_table[] = {\n",
           JSW * 8, ATOM_ALIGN);

    dump_atoms(s);

    s->global_object_offset = define_props(s, global_obj, PROPS_KIND_GLOBAL, NULL);

    printf("};\n\n");

    dump_cfuncs(s);
    
    printf("#ifndef JS_CLASS_COUNT\n"
           "#define JS_CLASS_COUNT JS_CLASS_USER /* total number of classes */\n"
           "#endif\n\n");

    dump_cfinalizers(s);

    free_class_entries(s);

    printf("const JSSTDLibraryDef %s = {\n", stdlib_name);
    printf("  js_stdlib_table,\n");
    printf("  js_c_function_table,\n");
    printf("  js_c_finalizer_table,\n");
    printf("  %d,\n", s->cur_offset);
    printf("  %d,\n", ATOM_ALIGN);
    printf("  %d,\n", s->sorted_atom_table_offset);
    printf("  %d,\n", s->global_object_offset);
    printf("  JS_CLASS_COUNT,\n");
    printf("};\n\n");

    return 0;
}
