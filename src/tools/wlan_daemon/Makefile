EXEC = aic_wlan_manager
OBJS = manager.o steering.o

include $(LINUX_CONFIG)

CONFIG_SEC_NLCMD = n

ifeq ($(CONFIG_SEC_NLCMD),y)
CFLAGS += -DCONFIG_SEC_NLCMD
endif

CFLAGS += -DCONFIG_WIFI_BAND_STEERING

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

romfs:
	$(ROMFSINST)    /bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o
