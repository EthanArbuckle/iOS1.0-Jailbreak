DRAW_IMAGE_DIR := draw_image
SCREENSHOT_DIR := screenshot
INJECTOR_DIR := injector
HACKTIVATION_HOOKS_DIR := hacktivation_hooks

RAMDISK_ROOT := ramdisk_root
RAMDISK_FILES := $(RAMDISK_ROOT)/files

DRAW_IMAGE_BIN := $(DRAW_IMAGE_DIR)/draw_image
SCREENSHOT_BIN := $(SCREENSHOT_DIR)/screenshot
INJECTOR_BIN := $(INJECTOR_DIR)/injector
HACKTIVATION_HOOKS_DYLIB := $(HACKTIVATION_HOOKS_DIR)/hacktivation.dylib

DRAW_IMAGE_DST := $(RAMDISK_FILES)/draw_image
SCREENSHOT_DST := $(RAMDISK_FILES)/screenshot
INJECTOR_DST := $(RAMDISK_FILES)/injector
HACKTIVATION_HOOKS_DST := $(RAMDISK_FILES)/hacktivation.dylib

BUILD_SCRIPT := build_ramdisk.py

.PHONY: all subprojects copy_bins ramdisk clean

all: ramdisk


subprojects:
	$(MAKE) -C $(DRAW_IMAGE_DIR)
	$(MAKE) -C $(SCREENSHOT_DIR)
	$(MAKE) -C $(INJECTOR_DIR)
	$(MAKE) -C $(HACKTIVATION_HOOKS_DIR)


copy_bins: subprojects
	cp $(DRAW_IMAGE_BIN) $(DRAW_IMAGE_DST)
	cp $(SCREENSHOT_BIN) $(SCREENSHOT_DST)
	cp $(INJECTOR_BIN) $(INJECTOR_DST)
	cp $(HACKTIVATION_HOOKS_DYLIB) $(HACKTIVATION_HOOKS_DST)


ramdisk: copy_bins
	python3 $(BUILD_SCRIPT)


clean:
	$(MAKE) -C $(SHOW_IMAGE_DIR) clean
	$(MAKE) -C $(SCREENSHOT_DIR) clean
	$(MAKE) -C $(INJECTOR_DIR) clean
	$(MAKE) -C $(HACKTIVATION_HOOKS_DIR) clean
	rm -f $(SHOW_IMAGE_DST)
	rm -f $(SCREENSHOT_DST)
	rm -f $(INJECTOR_DST)
	rm -f $(HACKTIVATION_HOOKS_DST)
