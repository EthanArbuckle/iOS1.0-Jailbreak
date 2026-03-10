#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <objc/runtime.h>
#include <objc/message.h>

typedef struct {
    float x;
    float y;
    float width;
    float height;
} CGRect;

typedef struct {
    id receiver;
    Class super_class;
} objc_super;

extern int UIApplicationMain(int argc, const char **argv, id principalClassName, id delegateClassName);

static id g_path = NULL;
static id g_img = NULL;
static id g_imgView = NULL;

id initWithArgc_argv(id self, SEL _cmd, int argc, const char **argv) {
    g_path = ((id (*)(id, SEL, const char *, int))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSString"), sel_registerName("alloc")), sel_registerName("initWithCString:encoding:"), argv[1], 1);

    id fileManager = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSFileManager"), sel_registerName("defaultManager"));
    id exists = ((id (*)(id, SEL, id))objc_msgSend)(fileManager, sel_registerName("fileExistsAtPath:"), g_path);
    if (exists == NULL) {
        printf("Error: Could not find file to display\n");
        ((void (*)(id, SEL))objc_msgSend)(self, sel_registerName("terminate"));
    }

    objc_super super_struct;
    super_struct.receiver = self;
    super_struct.super_class = class_getSuperclass(object_getClass(self));
    return ((id (*)(objc_super *, SEL, int, const char **))objc_msgSendSuper)(&super_struct, sel_registerName("_initWithArgc:argv:"), argc, argv);
}

void applicationDidFinishLaunching(id self, SEL _cmd, id notification) {
    CGRect screenRect;
    ((void (*)(CGRect *, id, SEL))objc_msgSend_stret)(&screenRect, objc_getClass("UIHardware"), sel_registerName("fullScreenApplicationContentRect"));
    
    screenRect.x = 0;
    screenRect.y = 0;
    id windowAlloc = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("UIWindow"), sel_registerName("alloc"));
    id window = ((id (*)(id, SEL, float, float, float, float))objc_msgSend)(windowAlloc, sel_registerName("initWithContentRect:"), screenRect.x, screenRect.y, screenRect.width, screenRect.height);

    id imageViewAlloc = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("UIImageView"), sel_registerName("alloc"));
    id g_imgView = ((id (*)(id, SEL, float, float, float, float))objc_msgSend)(imageViewAlloc, sel_registerName("initWithFrame:"), screenRect.x, screenRect.y, screenRect.width, screenRect.height);

    id image = ((id (*)(id, SEL, id))objc_msgSend)(objc_getClass("UIImage"), sel_registerName("imageAtPath:"), g_path);
    g_img = ((id (*)(id, SEL))objc_msgSend)(image, sel_registerName("retain"));

    ((void (*)(id, SEL, id))objc_msgSend)(g_imgView, sel_registerName("setImage:"), g_img);
    ((void (*)(id, SEL, int))objc_msgSend)(g_imgView, sel_registerName("setEnabledGestures:"), 0);

    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("orderFront:"), self);
    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("makeKey:"), self);
    ((void (*)(id, SEL, int))objc_msgSend)(window, sel_registerName("_setHidden:"), 0);
    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("setContentView:"), g_imgView);
}

int main(int argc, const char **argv) {
    if (argc <= 1) {
        printf("Usage: %s picture\n", argv[0]);
        exit(-1);
    }
    
    id poolAlloc = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSAutoreleasePool"), sel_registerName("alloc"));
    id pool = ((id (*)(id, SEL))objc_msgSend)(poolAlloc, sel_registerName("init"));

    Class _UIApplicationClass = objc_getClass("UIApplication");
    Class showImageAppClass = objc_allocateClassPair(_UIApplicationClass, "ShowImageApp", 0);
    class_addMethod(showImageAppClass, sel_registerName("_initWithArgc:argv:"), (IMP)initWithArgc_argv, "@@:i^*");
    class_addMethod(showImageAppClass, sel_registerName("applicationDidFinishLaunching:"), (IMP)applicationDidFinishLaunching, "v@:@");
    objc_registerClassPair(showImageAppClass);

    id principalClass = ((id (*)(id, SEL))objc_msgSend)((id)showImageAppClass, sel_registerName("class"));

    return UIApplicationMain(argc, argv, principalClass, NULL);
}
