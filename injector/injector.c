#include <dlfcn.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARM_THREAD_STATE 1

struct arm_thread_state {
    unsigned int r[13];
    unsigned int sp;
    unsigned int lr;
    unsigned int pc;
    unsigned int cpsr;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: %s <pid> <dylib_path>\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    char *dylib_path = argv[2];
    size_t path_len = strlen(dylib_path) + 1;

    void *handle = dlopen(NULL, RTLD_LAZY);
    void *real_dlopen = dlsym(handle, "dlopen");
    unsigned int dlopen_addr = (unsigned int)real_dlopen;

    mach_port_t task = MACH_PORT_NULL;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("task_for_pid failed: %s\n", mach_error_string(kr));
        return 1;
    }

    thread_act_array_t threads;
    mach_msg_type_number_t thread_count;
    kr = task_threads(task, &threads, &thread_count);
    if (kr != KERN_SUCCESS) {
        printf("task_threads failed: %s\n", mach_error_string(kr));
        return 1;
    }

    thread_act_t target_thread = threads[0];
    kr = thread_suspend(target_thread);
    if (kr != KERN_SUCCESS) {
        printf("thread_suspend failed: %s\n", mach_error_string(kr));
        return 1;
    }

    struct arm_thread_state orig_state;
    mach_msg_type_number_t state_count = sizeof(orig_state) / sizeof(unsigned int);
    kr = thread_get_state(target_thread, ARM_THREAD_STATE, (thread_state_t)&orig_state, &state_count);
    if (kr != KERN_SUCCESS) {
        printf("thread_get_state failed: %s\n", mach_error_string(kr));
        return 1;
    }

    mach_vm_address_t path_addr = 0;
    kr = vm_allocate(task, (vm_address_t *)&path_addr, 0x1000, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        printf("vm_allocate path failed: %s\n", mach_error_string(kr));
        return 1;
    }

    kr = vm_write(task, (vm_address_t)path_addr, (vm_offset_t)dylib_path, path_len);
    if (kr != KERN_SUCCESS) {
        printf("vm_write failed: %s\n", mach_error_string(kr));
        return 1;
    }

    mach_vm_address_t code_addr = 0;
    kr = vm_allocate(task, (vm_address_t *)&code_addr, 0x1000, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        printf("vm_allocate code failed: %s\n", mach_error_string(kr));
        return 1;
    }

    kr = vm_protect(task, (vm_address_t)code_addr, 0x1000, 0, VM_PROT_READ | VM_PROT_EXECUTE | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS) {
        printf("vm_protect failed: %s\n", mach_error_string(kr));
        return 1;
    }

    mach_vm_address_t data_addr = 0;
    kr = vm_allocate(task, (vm_address_t *)&data_addr, 0x1000, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        printf("vm_allocate data failed: %s\n", mach_error_string(kr));
        return 1;
    }

    unsigned int data[] = {
        (unsigned int)orig_state.lr,
        (unsigned int)path_addr,
        dlopen_addr,
    };

    kr = vm_write(task, (vm_address_t)data_addr, (vm_offset_t)data, sizeof(data));
    if (kr != KERN_SUCCESS) {
        printf("vm_write data failed: %s\n", mach_error_string(kr));
        return 1;
    }

    unsigned int da = (unsigned int)data_addr;
    unsigned int shellcode[] = {
        0xe59f4014,
        0xe5940004,
        0xe3a01002,
        0xe594c008,
        0xe12fff3c,
        0xe3a00000,
        0xe594f000,
        da,
    };

    kr = vm_write(task, (vm_address_t)code_addr, (vm_offset_t)shellcode, sizeof(shellcode));
    if (kr != KERN_SUCCESS) {
        printf("vm_write code failed: %s\n", mach_error_string(kr));
        return 1;
    }

    struct arm_thread_state new_state = orig_state;
    new_state.lr = (unsigned int)code_addr;

    kr = thread_set_state(target_thread, ARM_THREAD_STATE, (thread_state_t)&new_state, sizeof(new_state) / sizeof(unsigned int));
    if (kr != KERN_SUCCESS) {
        printf("thread_set_state failed: %s\n", mach_error_string(kr));
        return 1;
    }

    kr = thread_resume(target_thread);
    if (kr != KERN_SUCCESS) {
        printf("thread_resume failed: %s\n", mach_error_string(kr));
        return 1;
    }

    printf("done\n");
    return 0;
}
