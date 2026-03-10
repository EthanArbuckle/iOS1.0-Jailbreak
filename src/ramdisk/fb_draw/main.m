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
        if (argc < 2) {
            printf("Usage: %s <image_path>\n", argv[0]);
            return -1;
        }

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
        if (!IOMobileFramebufferOpen || !IOMobileFramebufferGetLayerDefaultSurface || !CoreSurfaceBufferLock || !CoreSurfaceBufferUnlock || !CoreSurfaceBufferGetBaseAddress || !IOServiceGetMatchingService || !IOServiceMatching) {
            printf("Failed to find necessary functions\n");
            return -1;
        }

        void *matching = ((void *(*)(const char *))IOServiceMatching)("AppleH1CLCD");
        io_service_t service = ((io_service_t (*)(mach_port_t, void *))IOServiceGetMatchingService)(0, matching);    
        if (!service) {
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

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(frameBuffer, displaySize.width, displaySize.height, 8, 4 * displaySize.width, colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        if (context == NULL) {
            printf("Couldn't create screen context!\n");
            return -1;
        }

        CGColorSpaceRelease(colorSpace);
        
        const char *image_path = argv[1];
        CGDataProviderRef provider = CGDataProviderCreateWithFilename(image_path);
        if (!provider) {
            printf("Failed to create data provider for image at path: %s\n", image_path);
            return -1;
        }

        CGImageRef image = CGImageCreateWithPNGDataProvider(provider, NULL, false, kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
        if (!image) {
            printf("Failed to create image from data provider\n");
            return -1;
        }

        CGContextDrawImage(context, CGRectMake(0, 0, displaySize.width, displaySize.height), image);
        CGContextFlush(context);
        
        ((int (*)(void *))CoreSurfaceBufferUnlock)(surfaceBuffer);

        CGImageRelease(image);

        sleep(10);
    }

    return 0;
}
