#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <syslog.h>

static void *cfstring_class = NULL;

static void (*orig_CFDictionarySetValue)(CFMutableDictionaryRef theDict, const void *key, const void *value);
static void hooked_CFDictionarySetValue(CFMutableDictionaryRef theDict, const void *key, const void *value) {
    if (cfstring_class == NULL) {
        CFStringRef tmp = CFSTR("x");
        cfstring_class = *(void **)tmp;
    }

    if (key != NULL && (unsigned int)key > 0x10000) {
        void *isa = *(void **)key;
        if (isa == cfstring_class) {
            CFRange found = CFStringFind((CFStringRef)key, CFSTR("ActivationState"), 0);
            if (found.location != kCFNotFound) {
                orig_CFDictionarySetValue(theDict, key, CFSTR("Activated"));
                return;
            }
        }
    }

    orig_CFDictionarySetValue(theDict, key, value);
}

static const void *(*orig_CFDictionaryGetValue)(CFDictionaryRef theDict, const void *key);
static const void *hooked_CFDictionaryGetValue(CFDictionaryRef theDict, const void *key) {
    if (cfstring_class == NULL) {
        CFStringRef tmp = CFSTR("x");
        cfstring_class = *(void **)tmp;
    }

    if (key != NULL && (unsigned int)key > 0x10000) {
        void *isa = *(void **)key;
        if (isa == cfstring_class) {
            CFRange found = CFStringFind((CFStringRef)key, CFSTR("ActivationState"), 0);
            if (found.location != kCFNotFound) {
                return CFSTR("Activated");
            }
        }
    }

    return orig_CFDictionaryGetValue(theDict, key);
}

static int (*orig_CTServerConnectionEnableBrickMode)(int a1, const void *cf, int brick);
static int hooked_CTServerConnectionEnableBrickMode(int a1, const void *cf, int brick) {
    // Disable baseband bricking
    return orig_CTServerConnectionEnableBrickMode(a1, cf, 0);
}

__attribute__((constructor)) static void init(void) {
	void *hooker = dlopen("/usr/lib/hooker.dylib", RTLD_NOW);
	if (hooker == NULL) {
		syslog(LOG_ERR, "failed to load hooker.dylib: %s", dlerror());
		return;
	}

	void *hook_function = dlsym(hooker, "hook_function");
	if (hook_function == NULL) {
		syslog(LOG_ERR, "failed to resolve hook_function: %s", dlerror());
		return;
	}

    void *_CTServerConnectionEnableBrickMode = dlsym(RTLD_DEFAULT, "_CTServerConnectionEnableBrickMode");
    if (_CTServerConnectionEnableBrickMode != NULL) {
        ((void (*)(void *, void *, void **))hook_function)(_CTServerConnectionEnableBrickMode, hooked_CTServerConnectionEnableBrickMode, (void **)&orig_CTServerConnectionEnableBrickMode);
    }

    ((void (*)(void *, void *, void **))hook_function)(CFDictionarySetValue, hooked_CFDictionarySetValue, (void **)&orig_CFDictionarySetValue);
    ((void (*)(void *, void *, void **))hook_function)(CFDictionaryGetValue, hooked_CFDictionaryGetValue, (void **)&orig_CFDictionaryGetValue);
}
