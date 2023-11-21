#===============================================================================
# export variables
#===============================================================================
ifeq ($(CFG_HI_EXPORT_FLAG),)
    ifneq ($(srctree),)
    KERNEL_DIR := $(srctree)

    SDK_DIR := $(shell cd $(KERNEL_DIR)/../../.. && /bin/pwd)
    else
    SDK_DIR := $(shell cd $(CURDIR)/../../../.. && /bin/pwd)
    endif
    include $(SDK_DIR)/base.mak
endif

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#   variable
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
objects :=

ifeq ($(CFG_HI_BT_DEVICE_REALTEK),y)
objects += rtkbt/driver
endif

ifeq ($(CFG_HI_BT_DEVICE_MT7668U),y)
objects += mt7668u/driver
endif

ifeq ($(CFG_HI_BT_DEVICE_AIC8800D),y)
objects += aicbt/driver
endif

ifneq ($(objects),)
obj-y += $(addsuffix /,$(objects))
else
$(error "If bluetooth support, you must choose one bluetooth device at least!!")
endif

objects_clean:=$(addsuffix _clean,$(objects))
objects_install:=$(addsuffix _install,$(objects))
objects_uninstall:=$(addsuffix _uninstall,$(objects))

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#   rules
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#==================================================================
#                          all
#==================================================================
.PHONY: all clean install uninstall
all: $(objects)

clean: $(objects_clean)

install: $(objects_install)

uninstall: $(objects_uninstall)

#==================================================================
#                          modules
#==================================================================
.PHONY: $(objects) $(objects_clean) $(objects_install) $(objects_uninstall)

$(objects):
	make -C $@ all

$(objects_clean):
	make -C $(patsubst %_clean,%,$@) clean

$(objects_install):
	make -C $(patsubst %_install,%,$@) install

$(objects_uninstall):
	make -C $(patsubst %_uninstall,%,$@) uninstall

