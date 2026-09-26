#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

extern "C" {

// Custom safe stdio implementations using direct file descriptors
size_t custom_bionic_fwrite(const void* ptr, size_t size, size_t nmemb, void* stream) {
    if (!ptr || size == 0 || nmemb == 0) return 0;
    ssize_t written = write(2, ptr, size * nmemb); // Write to stderr fd 2
    return (written > 0) ? (written / size) : 0;
}

int custom_bionic_fflush(void* stream) {
    return 0; // No-op on raw file descriptor
}

int custom_bionic_fprintf(void* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = vdprintf(2, fmt, ap);
    va_end(ap);
    return res;
}

int custom_bionic_vfprintf(void* stream, const char* fmt, va_list ap) {
    return vdprintf(2, fmt, ap);
}

int custom_bionic_fputc(int c, void* stream) {
    unsigned char ch = (unsigned char)c;
    return write(2, &ch, 1);
}

// Runtime GOT patcher
void HookBarhopperStdio(void* handle) {
    if (!handle) return;
    struct link_map* map = (struct link_map*)handle;
    uintptr_t base = (uintptr_t)map->l_addr;
    uintptr_t page_size = 4096;

#if defined(__aarch64__) || defined(_M_ARM64)
    // GOT offsets in ARM64 libbarhopper_v3.so (Jetson Orin Nano / Cortex-A78AE):
    // fwrite:   0x4aed20
    // fflush:   0x4aed30
    // fprintf:  0x4aee50
    // vfprintf: 0x4aefb8
    // fputc:    0x4aefc0
    uintptr_t got_start = base + 0x4aed00;
    uintptr_t page_start = got_start & ~(page_size - 1);
    mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_WRITE);

    *(void**)(base + 0x4aed20) = (void*)custom_bionic_fwrite;
    *(void**)(base + 0x4aed30) = (void*)custom_bionic_fflush;
    *(void**)(base + 0x4aee50) = (void*)custom_bionic_fprintf;
    *(void**)(base + 0x4aefb8) = (void*)custom_bionic_vfprintf;
    *(void**)(base + 0x4aefc0) = (void*)custom_bionic_fputc;
#elif defined(__x86_64__) || defined(_M_X64)
    // GOT offsets in x86_64 libbarhopper_v3.so:
    // fwrite:   0x599c68
    // fflush:   0x599c78
    // fprintf:  0x599d70
    // vfprintf: 0x599f18
    // fputc:    0x599f20
    uintptr_t got_start = base + 0x599c00;
    uintptr_t page_start = got_start & ~(page_size - 1);
    mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_WRITE);

    *(void**)(base + 0x599c68) = (void*)custom_bionic_fwrite;
    *(void**)(base + 0x599c78) = (void*)custom_bionic_fflush;
    *(void**)(base + 0x599d70) = (void*)custom_bionic_fprintf;
    *(void**)(base + 0x599f18) = (void*)custom_bionic_vfprintf;
    *(void**)(base + 0x599f20) = (void*)custom_bionic_fputc;
#endif
}

}

