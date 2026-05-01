# For 6502

ifeq ($(TARGET_6502), 1)
CXX=../llvm-mos/bin/mos-c64-clang++
else
CXX=clang++
endif

CFLAGS=-I.
LDFLAGS=

ifeq ($(TARGET_6502), 1)
CFLAGS+=-DTARGET_6502=1 -DLISP_HEAP_SIZE=13312
endif

ifeq ($(TARGET_6502), 1)
STANDARD=-std=c++20
OPTIMIZATION=-Oz
SANITIZE=
else
STANDARD=-std=c++20
OPTIMIZATION=-O3
SANITIZE=-fsanitize=address
endif

ifeq ($(DEBUG), 1)
	CFLAGS+=-g -DDEBUG $(SANITIZE)
else
	CFLAGS+=$(OPTIMIZATION) -flto -foptimize-sibling-calls
	LDFLAGS+=-flto
endif

ifeq ($(TARGET_LIBC_IO), 1)
	CFLAGS+=-DTARGET_LIBC_IO
endif

PROGRAMS=lispirito
DEPENDENCIES+=main.o LispNode.o extra.o operators.o circular_queue.o RCPointer.o Allocator.o

ifeq ($(TARGET_C64), 1)
DEPENDENCIES+=c64_terminal.o c64_io.o
CFLAGS+=-DTARGET_C64
endif

CPPFLAGS=$(STANDARD) $(CFLAGS)

all: $(PROGRAMS)

lispirito: $(DEPENDENCIES)
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

clean:
	rm -f *.o $(PROGRAMS)
