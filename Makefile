# For 6502

ifeq ($(TARGET_6502), 1)
CXX=../llvm-mos/bin/mos-hm6502-clang++
else
CXX=clang++
endif

CFLAGS=-I.
LDFLAGS=

ifeq ($(TARGET_6502), 1)
CFLAGS+= -DTARGET_6502=1 -DLISP_HEAP_SIZE=16384
endif

ifeq ($(TARGET_6502), 1)
STANDARD=-std=c++20
OPTIMIZATION=-Oz
SANITIZE=
else
STANDARD=-std=c++20
OPTIMIZATION=-O3
SANITIZE= -fsanitize=address
endif

CPPFLAGS=$(STANDARD) $(CFLAGS)

ifeq ($(DEBUG), 1)
	CFLAGS+=-g -DDEBUG $(SANITIZE)
else
	CFLAGS+=$(OPTIMIZATION) -flto
	LDFLAGS+=-flto
endif

PROGRAMS=lispirito
DEPENDENCIES=main.o LispNode.o extra.o operators.o

ifeq ($(SIMPLE_ALLOCATOR), 1)
DEPENDENCIES+=SimpleAllocator.o
CFLAGS += -DSIMPLE_ALLOCATOR
endif

all: $(PROGRAMS)

lispirito: $(DEPENDENCIES)
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

clean:
	rm -f *.o $(PROGRAMS)
