#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ANDROID_LOG_UNKNOWN 0
#define ANDROID_LOG_DEFAULT 1
#define ANDROID_LOG_VERBOSE 2
#define ANDROID_LOG_DEBUG   3
#define ANDROID_LOG_INFO    4
#define ANDROID_LOG_WARN    5
#define ANDROID_LOG_ERROR   6
#define ANDROID_LOG_FATAL   7
#define ANDROID_LOG_SILENT  8

extern "C" {

int* __errno() {
    return &errno;
}

// Android Bionic stdin/stdout/stderr table (__sF)
struct BionicFile {
    char _pad[256];
};
static BionicFile sF_table[3];
void* __sF = sF_table;

// POSIX Direct-FD stdio bypass to prevent glibc invalid handle abort
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || size == 0 || nmemb == 0) return 0;
    ssize_t written = write(2, ptr, size * nmemb);
    return (written > 0) ? (written / size) : 0;
}

int fputs(const char* s, FILE* stream) {
    if (!s) return 0;
    return write(2, s, strlen(s));
}

int fputc(int c, FILE* stream) {
    unsigned char ch = (unsigned char)c;
    return write(2, &ch, 1);
}

int fflush(FILE* stream) {
    return 0;
}

int fprintf(FILE* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = vdprintf(2, fmt, ap);
    va_end(ap);
    return res;
}

int vfprintf(FILE* stream, const char* fmt, va_list ap) {
    return vdprintf(2, fmt, ap);
}

int fstat(int fd, struct stat *buf) {
    return fstatat(fd, "", buf, AT_EMPTY_PATH);
}

int stat(const char *pathname, struct stat *buf) {
    return fstatat(AT_FDCWD, pathname, buf, 0);
}

int lstat(const char *pathname, struct stat *buf) {
    return fstatat(AT_FDCWD, pathname, buf, AT_SYMLINK_NOFOLLOW);
}

int __android_log_write(int prio, const char *tag, const char *text) {
    const char *prio_str = "INFO";
    if (prio == ANDROID_LOG_WARN) prio_str = "WARN";
    else if (prio >= ANDROID_LOG_ERROR) prio_str = "ERROR";
    else if (prio <= ANDROID_LOG_DEBUG) prio_str = "DEBUG";

    return dprintf(2, "[AndroidLog:%s][%s] %s\n", prio_str, tag ? tag : "default", text ? text : "");
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return __android_log_write(prio, tag, buf);
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    return __android_log_write(prio, tag, buf);
}

int __system_property_get(const char *name, char *value) {
    if (value) value[0] = '\0';
    return 0;
}

void android_set_abort_message(const char* msg) {
    dprintf(2, "[AndroidAbortMessage] %s\n", msg ? msg : "");
}

bool _ZN6tflite16UseGemmlowpOnX86Ev() { return false; }
void _ZN4absl19leak_check_internal12DoIgnoreLeakEPKv(const void* ptr) {}
bool _ZN4base33HasDuplicateGlobalSymbolsInternalEv() { return false; }
void __gcov_dump() {}
void __gcov_flush() {}
void MallocExtension_Internal_MarkThreadBusy() {}
void MallocExtension_Internal_MarkThreadIdle() {}
void* OPENSSL_memory_alloc(size_t size) { return malloc(size); }
void OPENSSL_memory_free(void* ptr) { free(ptr); }
size_t OPENSSL_memory_get_size(void* ptr) { return 0; }

void safe_exit(int code) {
    _exit(code);
}

}
