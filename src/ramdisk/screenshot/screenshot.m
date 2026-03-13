#include <Foundation/Foundation.h>
#include <dlfcn.h>
#include <UIKit/UIKit.h>
#include <stdint.h>
#include <mach/mach.h>

typedef mach_port_t io_connect_t;
typedef mach_port_t io_service_t;
typedef mach_port_t task_port_t;

int main(int argc, char *argv[]) {
    @autoreleasepool {
        dlopen("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", 2);
        dlopen("/System/Library/PrivateFrameworks/CoreSurface.framework/CoreSurface", 2);
        dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", 2);

        void *IOMobileFramebufferOpen = dlsym(RTLD_DEFAULT, "IOMobileFramebufferOpen");
        void *IOMobileFramebufferGetLayerDefaultSurface = dlsym(RTLD_DEFAULT, "IOMobileFramebufferGetLayerDefaultSurface");
        void *CoreSurfaceBufferLock = dlsym(RTLD_DEFAULT, "CoreSurfaceBufferLock");
        void *CoreSurfaceBufferUnlock = dlsym(RTLD_DEFAULT, "CoreSurfaceBufferUnlock");
        void *IOMobileFramebufferGetDisplaySize = dlsym(RTLD_DEFAULT, "IOMobileFramebufferGetDisplaySize");
        void *CoreSurfaceBufferGetBaseAddress = dlsym(RTLD_DEFAULT, "CoreSurfaceBufferGetBaseAddress");
        void *IOServiceGetMatchingService = dlsym(RTLD_DEFAULT, "IOServiceGetMatchingService");
        void *IOServiceMatching = dlsym(RTLD_DEFAULT, "IOServiceMatching");
        if (IOMobileFramebufferOpen == NULL || IOMobileFramebufferGetLayerDefaultSurface == NULL || CoreSurfaceBufferLock == NULL || CoreSurfaceBufferUnlock == NULL || CoreSurfaceBufferGetBaseAddress == NULL || IOServiceGetMatchingService == NULL || IOServiceMatching == NULL) {
            printf("Failed to find necessary functions\n");
            return -1;
        }

        void *matching = ((void * (*)(const char *))IOServiceMatching)("AppleH1CLCD");
        io_service_t service = ((io_service_t (*)(mach_port_t, void *))IOServiceGetMatchingService)(0, matching);
        if (service == 0) {
            printf("Couldn't find framebuffer service!\n");
            return -1;
        }

        io_connect_t conn;
        ((int (*)(io_service_t, task_port_t, uint32_t, io_connect_t *))IOMobileFramebufferOpen)(service, mach_task_self(), 0, &conn);

        void *surfaceBuffer = NULL;
        ((int (*)(io_service_t, int, void *))IOMobileFramebufferGetLayerDefaultSurface)(conn, 0, &surfaceBuffer);

        CGSize displaySize;
        ((void (*)(io_service_t, void *))IOMobileFramebufferGetDisplaySize)(conn, &displaySize);

        ((int (*)(void *, unsigned int))CoreSurfaceBufferLock)(surfaceBuffer, 3);
        void *frameBuffer = ((void * (*)(void *))CoreSurfaceBufferGetBaseAddress)(surfaceBuffer);

        uint32_t width = (uint32_t)displaySize.width;
        uint32_t height = (uint32_t)displaySize.height;
        uint32_t rowSize = width * 3;
        uint32_t padding = (4 - (rowSize % 4)) % 4;
        uint32_t stride = rowSize + padding;
        uint32_t pixelDataSize = stride * height;
        uint32_t fileSize = 54 + pixelDataSize;

        NSMutableData *bmpData = [NSMutableData dataWithCapacity:fileSize];

        uint8_t header[54] = {0};
        header[0] = 'B';
        header[1] = 'M';
        header[2] = fileSize & 0xFF;
        header[3] = (fileSize >> 8) & 0xFF;
        header[4] = (fileSize >> 16) & 0xFF;
        header[5] = (fileSize >> 24) & 0xFF;
        header[10] = 54;
        header[14] = 40;
        header[18] = width & 0xFF;
        header[19] = (width >> 8) & 0xFF;
        header[20] = (width >> 16) & 0xFF;
        header[21] = (width >> 24) & 0xFF;
        header[22] = height & 0xFF;
        header[23] = (height >> 8) & 0xFF;
        header[24] = (height >> 16) & 0xFF;
        header[25] = (height >> 24) & 0xFF;
        header[26] = 1;
        header[28] = 24;
        [bmpData appendBytes:header length:54];

        uint8_t *srcPixels = (uint8_t *)frameBuffer;
        uint8_t *rowBuf = (uint8_t *)malloc(stride);
        for (uint32_t y = 0; y < height; y++) {
            uint8_t *srcRow = srcPixels + (height - 1 - y) * width * 4;
            for (uint32_t x = 0; x < width; x++) {
                rowBuf[x * 3 + 0] = srcRow[x * 4 + 0];
                rowBuf[x * 3 + 1] = srcRow[x * 4 + 1];
                rowBuf[x * 3 + 2] = srcRow[x * 4 + 2];
            }
            
            for (uint32_t p = 0; p < padding; p++) {
                rowBuf[rowSize + p] = 0;
            }

            [bmpData appendBytes:rowBuf length:stride];
        }
        free(rowBuf);

        ((int (*)(void *))CoreSurfaceBufferUnlock)(surfaceBuffer);

        BOOL written = [bmpData writeToFile:@"/tmp/screenshot.bmp" atomically:YES];
        if (written == NO) {
            printf("Failed to write screenshot!\n");
            return -1;
        }

        printf("Screenshot saved to /tmp/screenshot.bmp\n");
    }

    return 0;
}
