MicroQuickJS
============

## Introduction

MicroQuickJS (aka. MQuickJS) is a JavaScript engine targeted at
embedded systems. It compiles and runs JavaScript programs using as little
as 10 kB of RAM. The whole engine requires about 100 kB of ROM (ARM
Thumb-2 code) including the C library. The speed is comparable to
QuickJS.

MQuickJS only supports a [subset](#javascript-subset-reference) of JavaScript close to ES5. It
implements a **stricter mode** where some error prone or inefficient
JavaScript constructs are forbidden.

Although MQuickJS shares much code with QuickJS, it internals are
different in order to consume less memory. In particular, it relies on
a tracing garbage collector, the VM does not rely on the CPU stack and
strings are stored in UTF-8.

## REPL

The REPL is `mqjs`. Usage:

```
usage: mqjs [options] [file [args]]
-h  --help            list options
-e  --eval EXPR       evaluate EXPR
-i  --interactive     go to interactive mode
-I  --include file    include an additional file
-d  --dump            dump the memory usage stats
    --memory-limit n  limit the memory usage to 'n' bytes
--no-column           no column number in debug information
-o FILE               save the bytecode to FILE
-m32                  force 32 bit bytecode output (use with -o)
-b  --allow-bytecode  allow bytecode in input file
```

Compile and run a program using 10 kB of RAM:

```sh
./mqjs --memory-limit 10k tests/mandelbrot.js
```


In addition to normal script execution, `mqjs` can output the compiled
bytecode to a persistent storage (file or ROM):

```sh
./mqjs -o mandelbrot.bin tests/mandelbrot.js
```

Then you can run the compiled bytecode as a normal script:

```sh
./mqjs -b mandelbrot.bin
```

The bytecode format depends on the endianness and word length (32 or
64 bit) of the CPU. On a 64 bit CPU, it is possible to use the option
`-m32` to generate 32 bit bytecode that can run on an embedded 32 bit
system.

Use the option `--no-column` to remove the column number debug info
(only line numbers are remaining) if you want to save some storage.

## Stricter mode

MQuickJS only supports a subset of JavaScript (mostly ES5). It is
always in **stricter** mode where some error prone JavaScript features
are disabled. The general idea is that the stricter mode is a subset
of JavaScript, so it still works as usual in other JavaScript
engines. Here are the main points:

- Only **strict mode** constructs are allowed, hence no `with` keyword
  and global variables must be declared with the `var` keyword.

- Arrays cannot have holes. Writing an element after the end is not
  allowed:
```js
    a = []
    a[0] = 1; // OK to extend the array length
    a[10] = 2; // TypeError
```
  If you need an array like object with holes, use a normal object
  instead:
```js
    a = {}
    a[0] = 1;
    a[10] = 2;
```
  `new Array(len)` still works as expected, but the array elements are
  initialized to `undefined`.
  Array literals with holes are a syntax error:
```js
    [ 1, , 3 ] // SyntaxError
```
- Only global `eval` is supported so it cannot access nor modify
  local variables:
```js
    eval('1 + 2'); // forbidden
    (1, eval)('1 + 2'); // OK
```
- No value boxing: `new Number(1)` is not supported and never
  necessary.

## JavaScript Subset Reference
 
- Only strict mode is supported with emphasis on ES5 compatibility.

- `Array` objects:

    - They have no holes.
    
    - Numeric properties are always handled by the array object and not
      forwarded to its prototype.
  
    - Out-of-bound sets are an error except when they are at the end of
      the array.
      
    - The `length` property is a getter/setter in the array prototype.

- all properties are writable, enumerable and configurable.

- `for in` only iterates over the object own properties. It should be
  used with this common pattern to have a consistent behavior with
  standard JavaScript:
  
```js
    for(var prop in obj) {
        if (obj.hasOwnProperty(prop)) {
            ...
        }
    }
```    
Always prefer using `for of` instead which is supported with arrays:

```js
    for(var prop of Object.keys(obj)) {
        ...
    }
```

- `prototype`, `length` and `name` are getter/setter in function objects.

- C functions cannot have their own properties (but C constructors
  behave as expected).

- The global object is supported, but its use is discouraged. It
  cannot contain getter/setters and properties directly created in it
  are not visible as global variables in the executing script.

- The variable associated with the `catch` keyword is a normal
  variable.

- Direct `eval` is not supported. Only indirect (=global) `eval` is
  supported.

- No value boxing (e.g. `new Number(1)` is not supported)

- Regexp:

    - case folding only works with ASCII characters.

    - the matching is unicode only i.e. `/./` matches a unicode code
      point instead of an UTF-16 character as with the `u` flag.

- String: `toLowerCase` / `toUpperCase` only handle ASCII characters.

- Date: only `Date.now()` is supported.

ES5 extensions:
  
- `for of` is supported but iterates only over arrays. No custom
   iterator is supported (yet).

- Typed arrays.

- `\u{hex}` is accepted in string literals

- Math functions: `imul`, `clz32`, `fround`, `trunc`, `log2`, `log10`.

- The exponentiation operator

- Regexp: the dotall (`s`), sticky (`y`) and unicode (`u`) flags are
  accepted. In unicode mode, the unicode properties are not supported.

- String functions: `codePointAt`, `replaceAll`, `trimStart`, `trimEnd`.

- The `globalThis` global property.

## C API

### Engine initialization

MQuickJS has almost no dependency on the C library. In particular it
does not use `malloc()`, `free()` nor `printf()`. When creating a
MQuickJS context, a memory buffer must be provided. The engine only
allocates memory in this buffer:
```c
    JSContext *ctx;
    uint8_t mem_buf[8192];
    ctx = JS_NewContext(mem_buf, sizeof(mem_buf), &js_stdlib);
    ...
    JS_FreeContext(ctx);
```
`JS_FreeContext(ctx)` is only necessary to call the finalizers of user
objects as no system memory is allocated by the engine.

### Memory handling

The C API is very similar to QuickJS (see `mquickjs.h`). However,
since there is a compacting garbage collector, there are important
differences:

1. Explicitly freeing values is not necessary (no `JS_FreeValue()`).

2. The address of objects can move each time a JS allocation is
called. The general rule is to avoid having variables of type
`JSValue` in C. They may be present only for temporary use between
MQuickJS API calls. In the other cases, always use a pointer to a
`JSValue`. `JS_PushGCRef()` returns a pointer to a temporary opaque
`JSValue` stored in a `JSGCRef` variable. `JS_PopGCRef()` must be used
to release the temporary reference. The opaque value in `JSGCRef` is
automatically updated when objects move. Example:

```c
JSValue my_js_func(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
        JSGCRef obj1_ref, obj2_ref;
        JSValue *obj1, *obj2, ret;

        ret = JS_EXCEPTION;
        obj1 = JS_PushGCRef(ctx, &obj1_ref);
        obj2 = JS_PushGCRef(ctx, &obj2_ref);
        *obj1 = JS_NewObject(ctx);
        if (JS_IsException(*obj1))
            goto fail;
        *obj2 = JS_NewObject(ctx); // obj1 may move
        if (JS_IsException(*obj2))
            goto fail;
        JS_SetPropertyStr(ctx, *obj1, "x", *obj2);  // obj1 and obj2 may move
        ret = *obj1;
     fail:
        PopGCRef(ctx, &obj2_ref);
        PopGCRef(ctx, &obj1_ref);
        return ret;
}
```

When running on a PC, the `DEBUG_GC` define can be used to force the
JS allocator to always move objects at each allocation. It is a good
way to check no invalid JSValue is used.

### Standard library

The standard library is compiled by a custom tool (`mquickjs_build.c`)
to C structures that may reside in ROM. Hence the standard library
instantiation is very fast and requires almost no RAM. An example of
standard library for `mqjs` is provided in `mqjs_stdlib.c`. The result
of its compilation is `mqjs_stdlib.h`.

`example.c` is a complete example using the MQuickJS C API.

### Persistent bytecode

The bytecode generated by `mqjs` may be executed from ROM. In this
case, it must be relocated before being flashed into ROM (see
`JS_RelocateBytecode()`). It is then instantiated with
`JS_LoadBytecode()` and run as normal script with `JS_Run()` (see
`mqjs.c`).

As with QuickJS, no backward compatibility is guaranteed at the
bytecode level. Moreover, the bytecode is not verified before being
executed. Only run JavaScript bytecode from trusted sources.

### Mathematical library and floating point emulation

MQuickJS contains its own tiny mathematical library (in
`libm.c`). Moreover, in case the CPU has no floating point support, it
contains its own floating point emulator which may be smaller than the
one provided with the GCC toolchain.

## Internals and comparison with QuickJS

### Garbage collection

A tracing and compacting garbage collector is used instead of
reference counting. It allows smaller objects. The GC adds an overhead
of a few bits per allocated memory block. Moreover, memory
fragmentation is avoided.

The engine has its own memory allocator and does not depend on the C
library malloc.

### Value and object representation

The value has the same size as a CPU word (hence 32 bits on a 32 bit
CPU). A value may contain:

  - a 31 bit integer (1 bit tag)

  - a single unicode codepoint (hence a string of one or two 16 bit code units)

  - a 64 bit floating point number with a small exponent with 64 bit CPU words

  - a pointer to a memory block. Memory blocks have a tag stored in
    memory.

JavaScript objects require at least 3 CPU words (hence 12 bytes on a
32 bit CPU). Additional data may be allocated depending on the object
class. The properties are stored in a hash table. Each property
requires at least 3 CPU words. Properties of standard library 
objects may reside in ROM.


Property keys are JSValues unlike QuickJS where they have a specific
type. They are either a string or a positive 31 bit integer. String
property keys are internalized (unique).

Strings are internally stored in WTF-8 (UTF-8 + unpaired surrogates)
instead of 8 or 16 bit arrays in QuickJS. Surrogate pairs are not
stored explicitly but are still visible when iterating thru 16 bit
code units in JavaScript. Hence full compatibility with JavaScript and
UTF-8 is maintained.

C functions can be stored as a single value to reduce the overhead. In
this case, no additional properties can be added. Most standard
library functions are stored this way.

### Standard library

The whole standard library resides in ROM. It is generated at compile
time. Only a few objects are created in RAM. Hence the engine
instantiation time is very low.

### Bytecode

It is a stack based bytecode (similar to QuickJS). However, the
bytecode references atoms thru an indirect table.

Line and column number information is compressed with 
[exponential-Golomb codes](https://en.wikipedia.org/wiki/Exponential-Golomb_coding).

### Compilation

The parser is very close to the QuickJS one but it avoids recursion so
the C stack usage is bounded. There is no abstract syntax tree. The
bytecode is generated in one pass with several tricks to optimize it
(QuickJS has several optimization passes).

## Tests and benchmarks

Running the basic tests:
```sh
make test
```

Running the QuickJS micro benchmark:
```sh
make microbench
```

Additional tests and a patched version of the Octane benchmark running
in stricter mode can be downloaded
[here](https://bellard.org/mquickjs/mquickjs-extras.tar.xz):

Running the V8 octane benchmark:
```sh
make octane
```

## License

MQuickJS is released under the MIT license.

Unless otherwise specified, the MQuickJS sources are copyright Fabrice
Bellard and Charlie Gordon.

