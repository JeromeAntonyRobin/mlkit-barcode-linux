#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

#include <errno.h>
int* __errno() {
    return &errno;
}


// Android Bionic __sF standard I/O streams table

#include <stdio.h>

// Android Bionic stdin/stdout/stderr table (__sF)
// In Android Bionic, stdin is &__sF[0], stdout is &__sF[1], stderr is &__sF[2]
struct BionicFile {
    unsigned char* _p;
    int _r;
    int _w;
    short _flags;
    short _file;
    // pad to 128 bytes
    char _pad[256];
};
static BionicFile sF_table[3];
void* __sF = sF_table;

int fflush(FILE* stream) {
    if (stream == (FILE*)&sF_table[0]) return ::fflush(stdin);
    if (stream == (FILE*)&sF_table[1]) return ::fflush(stdout);
    if (stream == (FILE*)&sF_table[2]) return ::fflush(stderr);
    return 0;
}



int __android_log_write(int prio, const char *tag, const char *text) {
    const char *prio_str = "INFO";
    if (prio == ANDROID_LOG_WARN) prio_str = "WARN";
    else if (prio >= ANDROID_LOG_ERROR) prio_str = "ERROR";
    else if (prio <= ANDROID_LOG_DEBUG) prio_str = "DEBUG";

    return fprintf(stderr, "[AndroidLog:%s][%s] %s\n", prio_str, tag ? tag : "default", text ? text : "");
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

// Android system property mock
int __system_property_get(const char *name, char *value) {
    if (value) {
        value[0] = '\0';
    }
    return 0;
}

void android_set_abort_message(const char* msg) {
    fprintf(stderr, "[AndroidAbortMessage] %s\n", msg ? msg : "");
}

// Missing internal Google symbols (Gemmlowp, Gcov, LeakCheck)
bool _ZN6tflite16UseGemmlowpOnX86Ev() {
    return false;
}

void _ZN4absl19leak_check_internal12DoIgnoreLeakEPKv(const void* ptr) {}
bool _ZN4base33HasDuplicateGlobalSymbolsInternalEv() { return false; }
void __gcov_dump() {}
void __gcov_flush() {}
void MallocExtension_Internal_MarkThreadBusy() {}
void MallocExtension_Internal_MarkThreadIdle() {}
void* OPENSSL_memory_alloc(size_t size) { return malloc(size); }
void OPENSSL_memory_free(void* ptr) { free(ptr); }
size_t OPENSSL_memory_get_size(void* ptr) { return 0; }

}

// Safe process termination to prevent Bionic stdio dtor collision
void safe_exit(int code) {
    _Exit(code);
}

extern "C" {

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (stream >= (FILE*)&sF_table[0] && stream <= (FILE*)&sF_table[2]) {
        return ::fwrite(ptr, size, nmemb, stderr);
    }
    return ::fwrite(ptr, size, nmemb, stream);
}

int fputs(const char* s, FILE* stream) {
    if (stream >= (FILE*)&sF_table[0] && stream <= (FILE*)&sF_table[2]) {
        return ::fputs(s, stderr);
    }
    return ::fputs(s, stream);
}

int fputc(int c, FILE* stream) {
    if (stream >= (FILE*)&sF_table[0] && stream <= (FILE*)&sF_table[2]) {
        return ::fputc(c, stderr);
    }
    return ::fputc(c, stream);
}

}

__attribute__((constructor)) static void init_bionic_stdio() {
    memcpy(&sF_table[0], stdin, sizeof(FILE));
    memcpy(&sF_table[1], stdout, sizeof(FILE));
    memcpy(&sF_table[2], stderr, sizeof(FILE));
}
