generated_sources := $(local-generated-sources-dir)

# Allow external configuration file
ifneq (,$(BOARD_CUSTOM_BT_CONFIG))
SRC := $(BOARD_CUSTOM_BT_CONFIG)
else
SRC := $(call my-dir)/include/$(addprefix vnd_, $(addsuffix .txt,$(basename $(TARGET_DEVICE))))
endif
ifeq (,$(wildcard $(SRC)))
# configuration file does not exist. Use default one
SRC := $(call my-dir)/include/vnd_generic.txt
endif
GEN := $(generated_sources)/vnd_buildcfg.h
TOOL := $(call my-dir)/gen-buildcfg.sh

$(GEN): PRIVATE_PATH := $(call my-dir)
$(GEN): PRIVATE_CUSTOM_TOOL = $(TOOL) $< $@
$(GEN): $(SRC)  $(TOOL)
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES += $(GEN)
