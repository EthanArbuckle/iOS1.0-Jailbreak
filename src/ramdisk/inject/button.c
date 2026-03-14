#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <unistd.h>

typedef uint32_t IOOptionBits;
typedef struct __IOHIDEvent *IOHIDEventRef;
typedef struct __IOHIDEventSystem *IOHIDEventSystemRef;

#define kIOHIDEventTypeKeyboard 3
#define kIOHIDEventFieldKeyboardDown 0x30002
#define kHIDPage_Consumer 0x0C
#define kHIDUsage_Csmr_Menu 0x40
#define kHIDUsage_Csmr_VolumeIncrement 0xE9
#define kHIDUsage_Csmr_VolumeDecrement 0xEA

static void *IOHIDEventGetIntegerValue = NULL;
static void *IOHIDEventCreateKeyboardEvent = NULL;
static void *IOHIDEventSystemCopyEvent = NULL;

static bool is_specific_button_pressed(IOHIDEventSystemRef eventSystem, uint32_t usagePage, uint32_t usage) {
    uint64_t ts = mach_absolute_time();
    IOHIDEventRef dummyEvent = ((IOHIDEventRef (*)(CFAllocatorRef, uint64_t, uint32_t, uint32_t, uint32_t, IOOptionBits))IOHIDEventCreateKeyboardEvent)(kCFAllocatorDefault, ts, usagePage, usage, 0, 0);
    if (dummyEvent == NULL) {
        return false;
    }

    IOHIDEventRef event = ((IOHIDEventRef (*)(IOHIDEventSystemRef, uint32_t, IOHIDEventRef, IOOptionBits))IOHIDEventSystemCopyEvent)(eventSystem, kIOHIDEventTypeKeyboard, dummyEvent, 0);
    CFRelease(dummyEvent);
    if (event == NULL) {
        return false;
    }

    CFIndex down = ((CFIndex (*)(IOHIDEventRef, uint32_t))IOHIDEventGetIntegerValue)(event, kIOHIDEventFieldKeyboardDown);
    CFRelease(event);
    return down != 0;
}

bool is_button_pressed(void) {
    dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", 2);

    void *IOHIDEventSystemCreate = dlsym(RTLD_DEFAULT, "IOHIDEventSystemCreate");
    void *IOHIDEventSystemOpen = dlsym(RTLD_DEFAULT, "IOHIDEventSystemOpen");
    IOHIDEventCreateKeyboardEvent = dlsym(RTLD_DEFAULT, "IOHIDEventCreateKeyboardEvent");
    IOHIDEventSystemCopyEvent = dlsym(RTLD_DEFAULT, "IOHIDEventSystemCopyEvent");
    IOHIDEventGetIntegerValue = dlsym(RTLD_DEFAULT, "IOHIDEventGetIntegerValue");

    if (IOHIDEventSystemCreate == NULL || IOHIDEventCreateKeyboardEvent == NULL || IOHIDEventSystemCopyEvent == NULL || IOHIDEventGetIntegerValue == NULL || IOHIDEventSystemOpen == NULL) {
        printf("Failed to find necessary functions\n");
        return -1;
    }

    IOHIDEventSystemRef eventSystem = ((IOHIDEventSystemRef (*)(CFAllocatorRef))IOHIDEventSystemCreate)(kCFAllocatorDefault);
    if (eventSystem == NULL) {
        printf("Failed to create IOHIDEventSystem\n");
        return -1;
    }

    int openResult = ((int (*)(IOHIDEventSystemRef, void *, void *, void *, void *))IOHIDEventSystemOpen)(eventSystem, 0, 0, 0, 0);
    if (openResult == 0) {
        printf("Failed to open IOHIDEventSystem\n");
        CFRelease(eventSystem);
        return -1;
    }

    // if Home or Volume Up are pressed, return true
    if (is_specific_button_pressed(eventSystem, kHIDPage_Consumer, kHIDUsage_Csmr_Menu) ||
        is_specific_button_pressed(eventSystem, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeIncrement)) {
        CFRelease(eventSystem);
        return true;
    }

    return 0;
}
