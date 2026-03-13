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

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("\n");
    printf("Jailbreak for iOS 1.x devices (S5L8900)\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help    Show this help message and exit\n");
    printf("  -s            Enable serial output (boot-args: serial=3)\n");
    printf("  -n            Normal boot (skip ramdisk, clear boot-args)\n");
    printf("\n");
    printf("Default behavior jailbreaks the device.\n");
}

int main(int argc, const char *argv[]) {
    bool enable_serial = false;
    bool normal_boot = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else if (strcmp(argv[i], "-s") == 0) {
            enable_serial = true;
        }
        else if (strcmp(argv[i], "-n") == 0) {
            normal_boot = true;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    idevice_t dev = {0};
    payload_t ramdisk_img = {0};
    payload_t bootlogo_png = {0};
    payload_t bootlogo_template_img = {0};
    exploit_image_t bootlogo_container = {0};
    uint8_t *ibootim_buf = NULL;
    size_t ibootim_size = 0;
    
    int exit_code = EXIT_FAILURE;
    do {
        printf("\nSearching for device...\n");
        STEP(idevice_open(&dev), "No device found in Recovery mode");
        printf("Device connected in %s mode\n", device_mode_string(dev.mode));
        
        // Load ramdisk and bootlogo files
        STEP(load_embedded_file("__ramdisk_img", &ramdisk_img), "Failed to load ramdisk image");
        STEP(load_embedded_file("__bootlogo_png", &bootlogo_png), "Failed to load bootlogo png");
        STEP(load_embedded_file("__template_img2", &bootlogo_template_img), "Failed to load bootlogo template");

        // Convert the bootlogo png to iBootIM format
        STEP(ibootim_png_to_raw(&bootlogo_png, &bootlogo_template_img, &ibootim_buf, &ibootim_size), "Failed to convert PNG to iBootIM format");
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
        
        const char *boot_args;
        if (normal_boot) {
            boot_args = "";
        }
        else if (enable_serial) {
            boot_args = "rd=md0 serial=3 -s -x pmd0=0x09CC2000.0x0133D000";
        }
        else {
            boot_args = "rd=md0 -s -x pmd0=0x09CC2000.0x0133D000";
        }

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "setenv boot-args \"%s\"\n", boot_args);
        STEP(idevice_send_command(&dev, cmd), "Failed to set boot-args");
        STEP(idevice_send_command(&dev, "saveenv\n"), "Failed to save environment");

        printf("Booting...\n");
        STEP(idevice_send_command(&dev, "bootx\n"), "Failed to execute bootx");
        idevice_send_command(&dev, "fsboot\n");

        exit_code = EXIT_SUCCESS;
    } while (0);
    
    idevice_close(&dev);
    payload_free(&ramdisk_img);
    payload_free(&bootlogo_png);
    payload_free(&bootlogo_template_img);
    exploit_image_free(&bootlogo_container);
    free(ibootim_buf);

    return exit_code;
}
