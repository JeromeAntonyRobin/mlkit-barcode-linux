#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include "jni.h"

// Android AOSP jni.h compatibility aliases
typedef JNINativeInterface JNINativeInterface_;
typedef _JNIEnv JNIEnv_;

// Concrete buffer structure to mock java.nio.ByteBuffer
struct MockDirectBuffer {
    void* address;
    jlong capacity;
};

// Generic mock array structure with length header
struct MockArrayHeader {
    uint32_t magic;      // 0xBAADF00D
    jsize    length;
    uint8_t  data[0];
};

#define ARRAY_MAGIC 0xBAADF00D

// Global storage to capture the output Protobuf byte array from Barhopper
static jbyte* g_last_byte_array = nullptr;
static jsize  g_last_byte_array_len = 0;

static void* Mock_GetDirectBufferAddress(JNIEnv* env, jobject buf) {
    if (!buf) return nullptr;
    MockDirectBuffer* db = reinterpret_cast<MockDirectBuffer*>(buf);
    return db->address;
}

static jlong Mock_GetDirectBufferCapacity(JNIEnv* env, jobject buf) {
    if (!buf) return 0;
    MockDirectBuffer* db = reinterpret_cast<MockDirectBuffer*>(buf);
    return db->capacity;
}

static void* Mock_GetPrimitiveArrayCritical(JNIEnv* env, jarray array, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!array) return nullptr;
    MockArrayHeader* h = reinterpret_cast<MockArrayHeader*>(array);
    if (h->magic == ARRAY_MAGIC) {
        return h->data;
    }
    return (void*)array;
}

static void Mock_ReleasePrimitiveArrayCritical(JNIEnv* env, jarray array, void* carray, jint mode) {
}

static jbyteArray Mock_NewByteArray(JNIEnv* env, jsize len) {
    g_last_byte_array_len = len;
    MockArrayHeader* h = (MockArrayHeader*)realloc(g_last_byte_array, sizeof(MockArrayHeader) + len);
    h->magic = ARRAY_MAGIC;
    h->length = len;
    g_last_byte_array = (jbyte*)h;
    return (jbyteArray)h;
}

static void Mock_SetByteArrayRegion(JNIEnv* env, jbyteArray array, jsize start, jsize len, const jbyte* buf) {
    if (!array || !buf) return;
    MockArrayHeader* h = reinterpret_cast<MockArrayHeader*>(array);
    if (h->magic == ARRAY_MAGIC) {
        memcpy(h->data + start, buf, len);
    }
}

static jbyte* Mock_GetByteArrayElements(JNIEnv* env, jbyteArray array, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!array) return nullptr;
    MockArrayHeader* h = reinterpret_cast<MockArrayHeader*>(array);
    if (h->magic == ARRAY_MAGIC) {
        return (jbyte*)h->data;
    }
    return (jbyte*)array;
}

static void Mock_ReleaseByteArrayElements(JNIEnv* env, jbyteArray array, jbyte* elems, jint mode) {
}

static jsize Mock_GetArrayLength(JNIEnv* env, jarray array) {
    if (!array) return 0;
    MockArrayHeader* h = reinterpret_cast<MockArrayHeader*>(array);
    if (h->magic == ARRAY_MAGIC) {
        return h->length;
    }
    return g_last_byte_array_len;
}

static void Mock_ExceptionClear(JNIEnv* env) {}
static jboolean Mock_ExceptionCheck(JNIEnv* env) { return JNI_FALSE; }

static jclass Mock_GetObjectClass(JNIEnv* env, jobject obj) {
    return (jclass)0x1;
}

enum MockObjectTag {
    OBJ_ROOT_OPTIONS = 0x100,
    OBJ_ONED_OPTIONS = 0x200,
    OBJ_MULTISCALE_DECODE = 0x300,
    OBJ_MULTISCALE_DETECT = 0x400
};

enum MockFieldType {
    FIELD_UNKNOWN = 0,
    // Root RecognitionOptions
    FIELD_BARCODE_FORMATS,
    FIELD_OUTPUT_UNRECOGNIZED,
    FIELD_USE_QR_MOBILENET_V3,
    FIELD_ENABLE_QR_ALIGNMENT_GRID,
    FIELD_ENABLE_KEYPOINT_FINDER,
    FIELD_USE_HALIDE_AFFINE,
    FIELD_QR_FOURTH_CORNER,
    FIELD_ONED_OPTIONS,
    FIELD_MULTISCALE_DECODE,
    FIELD_MULTISCALE_DETECT,

    // OnedRecognitionOptions
    FIELD_ONED_EAN13_UPCA_CONSISTENT,
    FIELD_ONED_EAN8_CONSISTENT,
    FIELD_ONED_UPCE_CONSISTENT,
    FIELD_ONED_CODE128_CONSISTENT,
    FIELD_ONED_CODE39_CONSISTENT,
    FIELD_ONED_CODE93_CONSISTENT,
    FIELD_ONED_ITF_CONSISTENT,
    FIELD_ONED_CODABAR_CONSISTENT,
    FIELD_ONED_CODE128_LEN,
    FIELD_ONED_CODE39_LEN,
    FIELD_ONED_CODE93_LEN,
    FIELD_ONED_ITF_LEN,
    FIELD_ONED_CODABAR_LEN,
    FIELD_ONED_CODE39_CHECK_DIGIT,
    FIELD_ONED_CODE39_EXTENDED,

    // MultiScaleDecodingOptions
    FIELD_MS_EXTRA_SCALES,
    FIELD_MS_MIN_DETECTED_DIM,
    FIELD_MS_SKIP_PROCESSING
};

static jfieldID Mock_GetFieldID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    // Root RecognitionOptions fields
    if (strcmp(name, "barcodeFormats") == 0) return (jfieldID)FIELD_BARCODE_FORMATS;
    if (strcmp(name, "outputUnrecognizedBarcodes") == 0) return (jfieldID)FIELD_OUTPUT_UNRECOGNIZED;
    if (strcmp(name, "useQrMobilenetV3") == 0) return (jfieldID)FIELD_USE_QR_MOBILENET_V3;
    if (strcmp(name, "enableQrAlignmentGrid") == 0) return (jfieldID)FIELD_ENABLE_QR_ALIGNMENT_GRID;
    if (strcmp(name, "enableUseKeypointAsFinderPattern") == 0) return (jfieldID)FIELD_ENABLE_KEYPOINT_FINDER;
    if (strcmp(name, "useHalideAffineCrop") == 0) return (jfieldID)FIELD_USE_HALIDE_AFFINE;
    if (strcmp(name, "qrEnableFourthCornerApproximation") == 0) return (jfieldID)FIELD_QR_FOURTH_CORNER;
    if (strcmp(name, "onedRecognitionOptions") == 0) return (jfieldID)FIELD_ONED_OPTIONS;
    if (strcmp(name, "multiScaleDecodingOptions") == 0) return (jfieldID)FIELD_MULTISCALE_DECODE;
    if (strcmp(name, "multiScaleDetectionOptions") == 0) return (jfieldID)FIELD_MULTISCALE_DETECT;

    // OnedRecognitionOptions fields
    if (strcmp(name, "ean13UpcaMinConsistentLines") == 0) return (jfieldID)FIELD_ONED_EAN13_UPCA_CONSISTENT;
    if (strcmp(name, "ean8MinConsistentLines") == 0) return (jfieldID)FIELD_ONED_EAN8_CONSISTENT;
    if (strcmp(name, "upceMinConsistentLines") == 0) return (jfieldID)FIELD_ONED_UPCE_CONSISTENT;
    if (strcmp(name, "code128MinConsistentLines") == 0) return (jfieldID)FIELD_ONED_CODE128_CONSISTENT;
    if (strcmp(name, "code39MinConsistentLines") == 0) return (jfieldID)FIELD_ONED_CODE39_CONSISTENT;
    if (strcmp(name, "code93MinConsistentLines") == 0) return (jfieldID)FIELD_ONED_CODE93_CONSISTENT;
    if (strcmp(name, "itfMinConsistentLines") == 0) return (jfieldID)FIELD_ONED_ITF_CONSISTENT;
    if (strcmp(name, "codabarMinConsistentLines") == 0) return (jfieldID)FIELD_ONED_CODABAR_CONSISTENT;
    if (strcmp(name, "code128MinCodeLength") == 0) return (jfieldID)FIELD_ONED_CODE128_LEN;
    if (strcmp(name, "code39MinCodeLength") == 0) return (jfieldID)FIELD_ONED_CODE39_LEN;
    if (strcmp(name, "code93MinCodeLength") == 0) return (jfieldID)FIELD_ONED_CODE93_LEN;
    if (strcmp(name, "itfMinCodeLength") == 0) return (jfieldID)FIELD_ONED_ITF_LEN;
    if (strcmp(name, "codabarMinCodeLength") == 0) return (jfieldID)FIELD_ONED_CODABAR_LEN;
    if (strcmp(name, "code39UseCheckDigit") == 0) return (jfieldID)FIELD_ONED_CODE39_CHECK_DIGIT;
    if (strcmp(name, "code39UseExtendedMode") == 0) return (jfieldID)FIELD_ONED_CODE39_EXTENDED;

    // MultiScale options fields
    if (strcmp(name, "extraScales") == 0) return (jfieldID)FIELD_MS_EXTRA_SCALES;
    if (strcmp(name, "minimumDetectedDimension") == 0) return (jfieldID)FIELD_MS_MIN_DETECTED_DIM;
    if (strcmp(name, "skipProcessingIfBarcodeFound") == 0) return (jfieldID)FIELD_MS_SKIP_PROCESSING;

    return (jfieldID)FIELD_UNKNOWN;
}

static jint Mock_GetIntField(JNIEnv* env, jobject obj, jfieldID fieldID) {
    switch ((uintptr_t)fieldID) {
        case FIELD_BARCODE_FORMATS: return 0xffff; // All barcode and QR formats enabled
        case FIELD_ONED_EAN13_UPCA_CONSISTENT: return 1;
        case FIELD_ONED_EAN8_CONSISTENT: return 3;
        case FIELD_ONED_UPCE_CONSISTENT: return 3;
        case FIELD_ONED_CODE128_CONSISTENT: return 1;
        case FIELD_ONED_CODE39_CONSISTENT: return 2;
        case FIELD_ONED_CODE93_CONSISTENT: return 2;
        case FIELD_ONED_ITF_CONSISTENT: return 3;
        case FIELD_ONED_CODABAR_CONSISTENT: return 2;
        case FIELD_ONED_CODE128_LEN: return 2;
        case FIELD_ONED_CODE39_LEN: return 2;
        case FIELD_ONED_CODE93_LEN: return 2;
        case FIELD_ONED_ITF_LEN: return 6;
        case FIELD_ONED_CODABAR_LEN: return 6;
        case FIELD_MS_MIN_DETECTED_DIM: return 10;
        default: return 0;
    }
}

static jboolean Mock_GetBooleanField(JNIEnv* env, jobject obj, jfieldID fieldID) {
    switch ((uintptr_t)fieldID) {
        case FIELD_OUTPUT_UNRECOGNIZED: return JNI_FALSE;
        case FIELD_USE_QR_MOBILENET_V3: return JNI_FALSE;
        case FIELD_ENABLE_QR_ALIGNMENT_GRID: return JNI_TRUE;
        case FIELD_ENABLE_KEYPOINT_FINDER: return JNI_TRUE;
        case FIELD_USE_HALIDE_AFFINE: return JNI_FALSE;
        case FIELD_QR_FOURTH_CORNER: return JNI_TRUE;
        case FIELD_ONED_CODE39_CHECK_DIGIT: return JNI_FALSE;
        case FIELD_ONED_CODE39_EXTENDED: return JNI_FALSE;
        case FIELD_MS_SKIP_PROCESSING: return JNI_TRUE;
        default: return JNI_FALSE;
    }
}

static jobject Mock_GetObjectField(JNIEnv* env, jobject obj, jfieldID fieldID) {
    switch ((uintptr_t)fieldID) {
        case FIELD_ONED_OPTIONS: return (jobject)OBJ_ONED_OPTIONS;
        case FIELD_MULTISCALE_DECODE: return (jobject)OBJ_MULTISCALE_DECODE;
        case FIELD_MULTISCALE_DETECT: return (jobject)OBJ_MULTISCALE_DETECT;
        // extraScales float[] array: return null so it uses default scales
        case FIELD_MS_EXTRA_SCALES: return nullptr;
        default: return nullptr;
    }
}

static jfloat* Mock_GetFloatArrayElements(JNIEnv* env, jfloatArray array, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!array) return nullptr;
    MockArrayHeader* h = reinterpret_cast<MockArrayHeader*>(array);
    if (h->magic == ARRAY_MAGIC) return (jfloat*)h->data;
    return (jfloat*)array;
}

static void Mock_ReleaseFloatArrayElements(JNIEnv* env, jfloatArray array, jfloat* elems, jint mode) {}

static JNINativeInterface_ g_native_interface;
static const JNINativeInterface_* g_native_interface_ptr = &g_native_interface;
static JNIEnv_ g_mock_env_obj;

extern "C" {

JNIEnv* GetMockJNIEnv() {
    static bool init = false;
    if (!init) {
        memset(&g_native_interface, 0, sizeof(JNINativeInterface_));
        g_native_interface.GetDirectBufferAddress = Mock_GetDirectBufferAddress;
        g_native_interface.GetDirectBufferCapacity = Mock_GetDirectBufferCapacity;
        g_native_interface.GetPrimitiveArrayCritical = Mock_GetPrimitiveArrayCritical;
        g_native_interface.ReleasePrimitiveArrayCritical = Mock_ReleasePrimitiveArrayCritical;
        g_native_interface.NewByteArray = Mock_NewByteArray;
        g_native_interface.SetByteArrayRegion = Mock_SetByteArrayRegion;
        g_native_interface.GetByteArrayElements = Mock_GetByteArrayElements;
        g_native_interface.ReleaseByteArrayElements = Mock_ReleaseByteArrayElements;
        g_native_interface.GetArrayLength = Mock_GetArrayLength;
        g_native_interface.GetFloatArrayElements = Mock_GetFloatArrayElements;
        g_native_interface.ReleaseFloatArrayElements = Mock_ReleaseFloatArrayElements;
        g_native_interface.ExceptionClear = Mock_ExceptionClear;
        g_native_interface.ExceptionCheck = Mock_ExceptionCheck;

        g_native_interface.GetObjectClass = Mock_GetObjectClass;
        g_native_interface.GetFieldID = Mock_GetFieldID;
        g_native_interface.GetIntField = Mock_GetIntField;
        g_native_interface.GetBooleanField = Mock_GetBooleanField;
        g_native_interface.GetObjectField = Mock_GetObjectField;

        g_mock_env_obj.functions = &g_native_interface;
        init = true;
    }
    return &g_mock_env_obj;
}

jbyte* GetLastProtobufResponse(jsize* out_len) {
    if (g_last_byte_array) {
        MockArrayHeader* h = reinterpret_cast<MockArrayHeader*>(g_last_byte_array);
        if (h->magic == ARRAY_MAGIC) {
            if (out_len) *out_len = h->length;
            return (jbyte*)h->data;
        }
    }
    if (out_len) *out_len = 0;
    return nullptr;
}

void* CreateDirectBuffer(void* data, jlong capacity) {
    MockDirectBuffer* db = new MockDirectBuffer();
    db->address = data;
    db->capacity = capacity;
    return (void*)db;
}

void FreeDirectBuffer(void* buf) {
    if (buf) {
        delete reinterpret_cast<MockDirectBuffer*>(buf);
    }
}

jbyteArray CreateMockByteArrayWithData(const void* src, jsize len) {
    MockArrayHeader* h = (MockArrayHeader*)malloc(sizeof(MockArrayHeader) + len);
    h->magic = ARRAY_MAGIC;
    h->length = len;
    if (src && len > 0) {
        memcpy(h->data, src, len);
    }
    return (jbyteArray)h;
}

void FreeMockByteArray(jbyteArray arr) {
    if (arr) free(arr);
}

}
