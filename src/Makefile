CC = clang
CFLAGS = -I../deps/include/
LDFLAGS = -L../deps/lib/ -lpng -lusb-1.0 -lcrypto -lz -framework CoreFoundation -framework IOKit -framework Security
TARGET = ios1-jb
SRC = file_payload.c ibootim.c idevice.c main.c s5l8900_exploit.c

BOOTLOGO_PNG = bootlogo/boot-logo.png
TEMPLATE_IMG2 = bootlogo/template.img2
RAMDISK_IMG = ramdisk/ramdisk.img

.PHONY: all clean ramdisk

all: $(TARGET)

ramdisk:
	$(MAKE) -C ramdisk

$(RAMDISK_IMG): ramdisk

$(TARGET): $(SRC) $(BOOTLOGO_PNG) $(TEMPLATE_IMG2) $(RAMDISK_IMG)
	@printf ".section __DATA,__bootlogo_png\n.globl _bootlogo_png_start\n_bootlogo_png_start:\n.incbin \"$(BOOTLOGO_PNG)\"\n" | $(CC) -c -x assembler - -o /dev/fd/3 3>&1 1>/dev/null | cat > /tmp/bp.o && \
	printf ".section __DATA,__template_img2\n.globl _template_img2_start\n_template_img2_start:\n.incbin \"$(TEMPLATE_IMG2)\"\n" | $(CC) -c -x assembler - -o /dev/fd/3 3>&1 1>/dev/null | cat > /tmp/ti.o && \
	printf ".section __DATA,__ramdisk_img\n.globl _ramdisk_img_start\n_ramdisk_img_start:\n.incbin \"$(RAMDISK_IMG)\"\n" | $(CC) -c -x assembler - -o /dev/fd/3 3>&1 1>/dev/null | cat > /tmp/rd.o && \
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC) /tmp/bp.o /tmp/ti.o /tmp/rd.o && \
	rm -f /tmp/bp.o /tmp/ti.o /tmp/rd.o

clean:
	$(MAKE) -C ramdisk clean
	rm -f $(TARGET)