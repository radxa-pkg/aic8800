#CROSS_COMPILE := /opt/toolchains/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-

LDFLAGS := -lpthread

default: all

all: wifi_test bt_test

wifi_test: wifi_test.o
	$(CROSS_COMPILE)gcc $+ -o $@

bt_test: bt_test.o
	$(CROSS_COMPILE)gcc $+ $(LDFLAGS) -o $@

%.o: %.c
	$(CROSS_COMPILE)gcc -c $+ -o $@

clean:
	rm *.o wifi_test bt_test

install:
	sudo cp wifi_test /sbin
	sudo cp bt_test /sbin