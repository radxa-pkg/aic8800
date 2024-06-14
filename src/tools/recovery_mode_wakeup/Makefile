#CROSS_COMPILE := /opt/toolchains/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-

LDFLAGS := -lpthread

default: all

all: stack_example

stack_example: stack_example.o
	$(CROSS_COMPILE)gcc $+ $(LDFLAGS) -o $@

%.o: %.c
	$(CROSS_COMPILE)gcc -c $+ -o $@

clean:
	rm *.o stack_example

install:
	sudo cp stack_example /sbin