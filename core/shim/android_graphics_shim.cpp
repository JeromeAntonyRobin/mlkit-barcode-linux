#include <stdint.h>
#include <stddef.h>

#define ANDROID_BITMAP_RESULT_SUCCESS            0
#define ANDROID_BITMAP_RESULT_BAD_PARAMETER    -1
#define ANDROID_BITMAP_RESULT_JNI_EXCEPTION    -2
#define ANDROID_BITMAP_RESULT_ALLOCATION_FAILED -3

#define ANDROID_BITMAP_FORMAT_RGBA_8888 1
#define ANDROID_BITMAP_FORMAT_RGB_565   4
#define ANDROID_BITMAP_FORMAT_RGBA_4444 7
#define ANDROID_BITMAP_FORMAT_A_8       8

struct AndroidBitmapInfo {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t  format;
    uint32_t flags;
};

struct MockBitmap {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t  format;
    void*    pixels;
};

extern "C" {

int AndroidBitmap_getInfo(void* env, void* jbitmap, AndroidBitmapInfo* info) {
    if (!jbitmap || !info) return ANDROID_BITMAP_RESULT_BAD_PARAMETER;
    MockBitmap* bm = reinterpret_cast<MockBitmap*>(jbitmap);
    info->width = bm->width;
    info->height = bm->height;
    info->stride = bm->stride;
    info->format = bm->format;
    info->flags = 0;
    return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_lockPixels(void* env, void* jbitmap, void** addrPtr) {
    if (!jbitmap || !addrPtr) return ANDROID_BITMAP_RESULT_BAD_PARAMETER;
    MockBitmap* bm = reinterpret_cast<MockBitmap*>(jbitmap);
    *addrPtr = bm->pixels;
    return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_unlockPixels(void* env, void* jbitmap) {
    return ANDROID_BITMAP_RESULT_SUCCESS;
}

}
