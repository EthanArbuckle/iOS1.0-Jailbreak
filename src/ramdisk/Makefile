SHOW_IMAGE_DIR := show_image
INJECTOR_DIR := injector
HACKTIVATION_HOOKS_DIR := hacktivation_hooks

RAMDISK_ROOT := ramdisk_root
RAMDISK_FILES := $(RAMDISK_ROOT)/files

SHOW_IMAGE_BIN := $(SHOW_IMAGE_DIR)/show_image
INJECTOR_BIN := $(INJECTOR_DIR)/injector
HACKTIVATION_HOOKS_DYLIB := $(HACKTIVATION_HOOKS_DIR)/hacktivation.dylib

SHOW_IMAGE_DST := $(RAMDISK_FILES)/show_image
INJECTOR_DST := $(RAMDISK_FILES)/injector
HACKTIVATION_HOOKS_DST := $(RAMDISK_FILES)/hacktivation.dylib

BUILD_SCRIPT := build_ramdisk.py

.PHONY: all subprojects copy_bins ramdisk clean

all: ramdisk


subprojects:
	$(MAKE) -C $(SHOW_IMAGE_DIR)
	$(MAKE) -C $(INJECTOR_DIR)
	$(MAKE) -C $(HACKTIVATION_HOOKS_DIR)


copy_bins: subprojects
	cp $(SHOW_IMAGE_BIN) $(SHOW_IMAGE_DST)
# 	cp $(INJECTOR_BIN) $(INJECTOR_DST)
# 	cp $(HACKTIVATION_HOOKS_DYLIB) $(HACKTIVATION_HOOKS_DST)


ramdisk: copy_bins
	python3 $(BUILD_SCRIPT)


clean:
	$(MAKE) -C $(SHOW_IMAGE_DIR) clean
	$(MAKE) -C $(INJECTOR_DIR) clean
	$(MAKE) -C $(HACKTIVATION_HOOKS_DIR) clean
	rm -f $(SHOW_IMAGE_DST)
	rm -f $(INJECTOR_DST)
	rm -f $(HACKTIVATION_HOOKS_DST)
