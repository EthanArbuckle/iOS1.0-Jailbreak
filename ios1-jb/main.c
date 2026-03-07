//
//  main.c
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/5/26.
//

#include <stdio.h>
#include <stdlib.h>
#include "s5l8900_exploit.h"
#include "file_payload.h"
#include "ibootim.h"
#include "idevice.h"

#define STEP(expr, msg) if ((expr) != KERN_SUCCESS) { fprintf(stderr, msg "\n"); break; }

int main(int argc, const char *argv[]) {
    // Path to the bootlogo png
    const char *bootlogo_png_path = "/Users/ethanarbuckle/Desktop/ios1-jb/bootlogo/boot-logo.png";
    // Path to the bootlogo template img2
    const char *template_bootlogo_img2_path = "/Users/ethanarbuckle/Desktop/ios1-jb/bootlogo/template.img2";
    // Path to the ramdisk img produced by build_ramdisk.py
    const char *ramdisk_path = "/Users/ethanarbuckle/Desktop/ios1-jb/ramdisk/ramdisk.img";
    
    int exit_code = EXIT_FAILURE;
    idevice_t dev = {0};
    payload_t ramdisk_img = {0};
    exploit_image_t bootlogo_container = {0};
    uint8_t *ibootim_buf = NULL;
    size_t ibootim_size = 0;
    
    do {
        printf("\nSearching for device...\n");
        STEP(idevice_open(&dev), "No device found in Recovery mode");
        printf("Device connected in %s mode\n", device_mode_string(dev.mode));
        
        // Load ramdisk and kernel images
        STEP(load_file(ramdisk_path, &ramdisk_img), "Failed to load ramdisk image. Did you run build_ramdisk.py?");

        // Convert the bootlogo png to iBootIM format
        STEP(ibootim_png_to_raw(bootlogo_png_path, template_bootlogo_img2_path, &ibootim_buf, &ibootim_size), "Failed to convert PNG to iBootIM format");
        // Wrap the image in an 8900 container with malformed cert chain that bypasses signature validation
        STEP(exploit_image_create(ibootim_buf, ibootim_size, &bootlogo_container), "Failed to create exploit image");
        
        // Send bootlogo
        STEP(idevice_init_handshake(&dev), "Handshake failed");
        STEP(idevice_send_file(&dev, bootlogo_container.buf, bootlogo_container.size, 0x09000000), "Failed to send logo");
        STEP(idevice_send_command(&dev, "setpicture 0\n"), "Failed to set picture");
        STEP(idevice_send_command(&dev, "bgcolor 0 0 0\n"), "Failed to set send bgcolor command");
        
        // Send ramdisk and set boot args. The ramdisk will jailbreak the device.
        // The device will boot unsigned ramdisks if they're loaded at an address > 0x9C000000
        printf("Sending ramdisk...\n");
        STEP(idevice_send_file(&dev, ramdisk_img.data, ramdisk_img.size, 0x09CC2000), "Failed to send ramdisk");
        STEP(idevice_send_command(&dev, "setenv boot-args \"rd=md0 -s -x pmd0=0x09CC2000.0x0133D000\"\n"), "Failed to set boot-args");
        STEP(idevice_send_command(&dev, "saveenv\n"), "Failed to save environment");

        printf("Booting...\n");
        STEP(idevice_send_command(&dev, "bootx\n"), "Failed to execute bootx");
        idevice_send_command(&dev, "fsboot\n");

        exit_code = EXIT_SUCCESS;
    } while (0);
    
    idevice_close(&dev);
    payload_free(&ramdisk_img);
    exploit_image_free(&bootlogo_container);
    free(ibootim_buf);

    return exit_code;
}
