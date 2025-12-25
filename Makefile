#CONFIG_PROFILE=y
#CONFIG_X86_32=y
#CONFIG_ARM32=y
#CONFIG_WIN32=y
#CONFIG_SOFTFLOAT=y
#CONFIG_ASAN=y
#CONFIG_GPROF=y
CONFIG_SMALL=y
# consider warnings as errors (for development)
#CONFIG_WERROR=y

ifdef CONFIG_ARM32
CROSS_PREFIX=arm-linux-gnu-
endif

ifdef CONFIG_WIN32
  ifdef CONFIG_X86_32
    CROSS_PREFIX?=i686-w64-mingw32-
  else
    CROSS_PREFIX?=x86_64-w64-mingw32-
  endif
  EXE=.exe
else
  CROSS_PREFIX?=
  EXE=
endif

HOST_CC=gcc
CC=$(CROSS_PREFIX)gcc
CFLAGS=-Wall -g -MMD -D_GNU_SOURCE -fno-math-errno -fno-trapping-math
HOST_CFLAGS=-Wall -g -MMD -D_GNU_SOURCE -fno-math-errno -fno-trapping-math
ifdef CONFIG_WERROR
CFLAGS+=-Werror
HOST_CFLAGS+=-Werror
endif
ifdef CONFIG_ARM32
CFLAGS+=-mthumb
endif
ifdef CONFIG_SMALL
CFLAGS+=-Os
else
CFLAGS+=-O2
endif
#CFLAGS+=-fstack-usage
ifdef CONFIG_SOFTFLOAT
CFLAGS+=-msoft-float
CFLAGS+=-DUSE_SOFTFLOAT
endif # CONFIG_SOFTFLOAT
HOST_CFLAGS+=-O2
LDFLAGS=-g
HOST_LDFLAGS=-g
ifdef CONFIG_GPROF
CFLAGS+=-p
LDFLAGS+=-p
endif
ifdef CONFIG_ASAN
CFLAGS+=-fsanitize=address -fno-omit-frame-pointer
LDFLAGS+=-fsanitize=address -fno-omit-frame-pointer
endif
ifdef CONFIG_X86_32
CFLAGS+=-m32
LDFLAGS+=-m32
endif
ifdef CONFIG_PROFILE
CFLAGS+=-p
LDFLAGS+=-p
endif

# when cross compiling from a 64 bit system to a 32 bit system, force
# a 32 bit output
ifdef CONFIG_X86_32
MQJS_BUILD_FLAGS=-m32
endif
ifdef CONFIG_ARM32
MQJS_BUILD_FLAGS=-m32
endif

PROGS=mqjs$(EXE) example$(EXE)
TEST_PROGS=dtoa_test libm_test 

all: $(PROGS)

MQJS_OBJS=mqjs.o readline_tty.o readline.o mquickjs.o dtoa.o libm.o cutils.o
LIBS=-lm

mqjs$(EXE): $(MQJS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

mquickjs.o: mquickjs_atom.h

mqjs_stdlib: mqjs_stdlib.host.o mquickjs_build.host.o
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $^

mquickjs_atom.h: mqjs_stdlib
	./mqjs_stdlib -a $(MQJS_BUILD_FLAGS) > $@

mqjs_stdlib.h: mqjs_stdlib
	./mqjs_stdlib $(MQJS_BUILD_FLAGS) > $@

mqjs.o: mqjs_stdlib.h

# C API example
example.o: example_stdlib.h

example$(EXE): example.o mquickjs.o dtoa.o libm.o cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

example_stdlib: example_stdlib.host.o mquickjs_build.host.o
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $^

example_stdlib.h: example_stdlib
	./example_stdlib $(MQJS_BUILD_FLAGS) > $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.host.o: %.c
	$(HOST_CC) $(HOST_CFLAGS) -c -o $@ $<

test: mqjs example
	./mqjs tests/test_closure.js
	./mqjs tests/test_language.js
	./mqjs tests/test_loop.js
	./mqjs tests/test_builtin.js
# test bytecode generation and loading
	./mqjs -o test_builtin.bin tests/test_builtin.js
#	@sha256sum -c test_builtin.sha256
	./mqjs test_builtin.bin
	./example tests/test_rect.js

microbench: mqjs
	./mqjs tests/microbench.js

octane: mqjs
	./mqjs --memory-limit 256M tests/octane/run.js

size: mqjs
	size mqjs mqjs.o readline.o cutils.o dtoa.o libm.o mquickjs.o

dtoa_test: tests/dtoa_test.o dtoa.o cutils.o tests/gay-fixed.o tests/gay-precision.o tests/gay-shortest.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

libm_test: tests/libm_test.o libm.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

rempio2_test: tests/rempio2_test.o libm.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f *.o *.d *~ tests/*.o tests/*.d tests/*~ test_builtin.bin mqjs_stdlib mqjs_stdlib.h mquickjs_build_atoms mquickjs_atom.h mqjs_example example_stdlib example_stdlib.h $(PROGS) $(TEST_PROGS)

-include $(wildcard *.d)
