#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef mach_port_t io_object_t;
typedef io_object_t io_service_t;
typedef kern_return_t IOReturn;

typedef int32_t HRESULT_T;

typedef struct {
    void *_reserved;
    HRESULT_T (*QueryInterface)(void *self, CFUUIDBytes iid, void **out);
    uint32_t (*AddRef)(void *self);
    uint32_t (*Release)(void *self);
} IOCFPlugInInterface;

static mach_port_t s_masterPort;
static CFUUIDRef s_plugInIfaceID;

static io_service_t (*pIOServiceGetMatchingService)(mach_port_t, CFDictionaryRef);
static CFMutableDictionaryRef (*pIOServiceMatching)(const char *);
static IOReturn (*pIORegistryEntrySetCFProperties)(io_service_t, CFTypeRef);
static IOReturn (*pIOCreatePlugInInterfaceForService)(io_service_t, CFUUIDRef, CFUUIDRef, IOCFPlugInInterface ***, int32_t *);
static IOReturn (*pIODestroyPlugInInterface)(IOCFPlugInInterface **);
static void (*pIOObjectRelease)(io_object_t);

static int load_iokit(void) {
    void *iokit = dlopen("/System/Library/Frameworks/IOKit.framework/Versions/A/IOKit", RTLD_LAZY);
    if (iokit == NULL) {
        fprintf(stderr, "dlopen IOKit: %s\n", dlerror());
        return -1;
    }

#define LOAD(sym)                                               \
    do {                                                        \
        *(void **)&p##sym = dlsym(iokit, #sym);                 \
        if (!p##sym) {                                          \
            fprintf(stderr, "dlsym %s: %s\n", #sym, dlerror()); \
            return -1;                                          \
        }                                                       \
    } while (0)

    LOAD(IOServiceGetMatchingService);
    LOAD(IOServiceMatching);
    LOAD(IORegistryEntrySetCFProperties);
    LOAD(IOCreatePlugInInterfaceForService);
    LOAD(IODestroyPlugInInterface);
    LOAD(IOObjectRelease);

#undef LOAD

    // kIOMasterPortDefault is an exported mach_port_t (usually 0)
    mach_port_t *pp = dlsym(iokit, "kIOMasterPortDefault");
    s_masterPort = pp ? *pp : MACH_PORT_NULL;

    // kIOCFPlugInInterfaceID = C244E858-109C-11D4-91D4-0050E4C6426F
    CFUUIDRef *pu = dlsym(iokit, "kIOCFPlugInInterfaceID");
    if (pu) {
        s_plugInIfaceID = *pu;
    }
    else {
        s_plugInIfaceID = CFUUIDGetConstantUUIDWithBytes(NULL, 0xC2, 0x44, 0xE8, 0x58, 0x10, 0x9C, 0x11, 0xD4, 0x91, 0xD4, 0x00, 0x50, 0xE4, 0xC6, 0x42, 0x6F);
    }

    return 0;
}

static CFUUIDRef kPluginType(void) {
    // plugin type for the USB device (gadget) family: 9E72217E-8A60-11DB-BF57-000D936D06D2
    return CFUUIDGetConstantUUIDWithBytes(NULL, 0x9E, 0x72, 0x21, 0x7E, 0x8A, 0x60, 0x11, 0xDB, 0xBF, 0x57, 0x00, 0x0D, 0x93, 0x6D, 0x06, 0xD2);
}

static CFUUIDRef kDevIface(void) {
    // device interface (QueryInterface target): EA33BA4F-8A60-11DB-84DB-000D936D06D2
    return CFUUIDGetConstantUUIDWithBytes(NULL, 0xEA, 0x33, 0xBA, 0x4F, 0x8A, 0x60, 0x11, 0xDB, 0x84, 0xDB, 0x00, 0x0D, 0x93, 0x6D, 0x06, 0xD2);
}

typedef IOReturn (*fn_self)(void *self);
typedef IOReturn (*fn_u8)(void *self, uint8_t v);

#define VT(dev) (*(void ***)(dev))
#define M_SELF(dev, i) ((fn_self)VT(dev)[(i)])
#define M_U8(dev, i) ((fn_u8)VT(dev)[(i)])

static void disable_watchdog(void) {
    io_service_t wd = pIOServiceGetMatchingService(s_masterPort, pIOServiceMatching("IOWatchDogTimer"));
    if (!wd) {
        fprintf(stderr, "no watchdog (ok)\n");
        return;
    }

    int32_t zero = 0;
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &zero);
    pIORegistryEntrySetCFProperties(wd, n);

    CFRelease(n);
    pIOObjectRelease(wd);
}

static void kick_mux(void) {
    io_service_t ipod = pIOServiceGetMatchingService(s_masterPort, pIOServiceMatching("IOIpodUSBDevice"));
    if (!ipod) {
        fprintf(stderr, "no IOIpodUSBDevice\n");
        return;
    }

    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(d, CFSTR("USBDeviceCommand"), CFSTR("SetSerialNumber"));
    CFDictionarySetValue(d, CFSTR("USBDeviceCommandParameter"), CFSTR("0000000000000000000000000000000000000000"));
    IOReturn kr = pIORegistryEntrySetCFProperties(ipod, d);
    if (kr) {
        fprintf(stderr, "kick_mux: SetSerialNumber returned 0x%x\n", kr);
    }

    CFRelease(d);
    pIOObjectRelease(ipod);
}

int main(void) {
    if (load_iokit()) {
        return 1;
    }

    disable_watchdog();

    CFMutableDictionaryRef match = pIOServiceMatching("IOUSBDeviceInterface");
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(props, CFSTR("USBDeviceFunction"), CFSTR("PTP"));
    CFDictionarySetValue(match, CFSTR("IOPropertyMatch"), props);
    CFRelease(props);

    io_service_t service = pIOServiceGetMatchingService(s_masterPort, match);
    if (!service) {
        fprintf(stderr, "PTP match failed, trying without property...\n");
        service = pIOServiceGetMatchingService(s_masterPort, pIOServiceMatching("IOUSBDeviceInterface"));
    }

    if (!service) {
        fprintf(stderr, "no IOUSBDeviceInterface service\n");
        return 1;
    }

    IOCFPlugInInterface **plugin = NULL;
    int32_t score = 0;
    IOReturn kr = pIOCreatePlugInInterfaceForService(service, kPluginType(), s_plugInIfaceID, &plugin, &score);
    pIOObjectRelease(service);
    if (kr || plugin == NULL) {
        fprintf(stderr, "plugin create: 0x%x\n", kr);
        return 1;
    }

    void *dev = NULL;
    HRESULT_T hr = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kDevIface()), &dev);
    pIODestroyPlugInInterface(plugin);
    if (hr || dev == NULL) {
        fprintf(stderr, "QueryInterface: 0x%lx\n", (long)hr);
        return 1;
    }

    // Open
    kr = M_U8(dev, 4)(dev, 0);
    if (kr) {
        fprintf(stderr, "open: 0x%x\n", kr);
        return 1;
    }

    // SetClass
    kr = M_U8(dev, 11)(dev, 255);
    if (kr) {
        fprintf(stderr, "SetClass: 0x%x\n", kr);
        return 1;
    }

    // SetSubClass
    kr = M_U8(dev, 12)(dev, 80);
    if (kr) {
        fprintf(stderr, "SetSubClass: 0x%x\n", kr);
        return 1;
    }

    // SetProtocol
    kr = M_U8(dev, 13)(dev, 67);
    if (kr) {
        fprintf(stderr, "SetProtocol: 0x%x\n", kr);
        return 1;
    }

    // CommitConfiguration
    kr = M_SELF(dev, 17)(dev);
    if (kr) {
        fprintf(stderr, "Commit: 0x%x\n", kr);
        return 1;
    }

    // return
    M_U8(dev, 5)(dev, 0);

    kick_mux();

    printf("USB is up\n");

    return 0;
}
