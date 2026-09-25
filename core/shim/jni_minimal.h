#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  jboolean;
typedef int8_t   jbyte;
typedef uint16_t jchar;
typedef int16_t  jshort;
typedef int32_t  jint;
typedef int64_t  jlong;
typedef float    jfloat;
typedef double   jdouble;
typedef jint     jsize;

typedef void* jobject;
typedef jobject jclass;
typedef jobject jthrowable;
typedef jobject jstring;
typedef jobject jarray;
typedef jarray jbooleanArray;
typedef jarray jbyteArray;
typedef jarray jcharArray;
typedef jarray jshortArray;
typedef jarray jintArray;
typedef jarray jlongArray;
typedef jarray jfloatArray;
typedef jarray jdoubleArray;
typedef jarray jobjectArray;

typedef void* jweak;
typedef union jvalue {
    jboolean z;
    jbyte    b;
    jchar    c;
    jshort   s;
    jint     i;
    jlong    j;
    jfloat   f;
    jdouble  d;
    jobject  l;
} jvalue;

typedef struct _jfieldID* jfieldID;
typedef struct _jmethodID* jmethodID;

struct JNINativeInterface_;
typedef const struct JNINativeInterface_* JNIEnv;

struct JNIInvokeInterface_;
typedef const struct JNIInvokeInterface_* JavaVM;

struct JNINativeInterface_ {
    void* reserved0;
    void* reserved1;
    void* reserved2;
    void* reserved3;

    jint (*GetVersion)(JNIEnv*);
    jclass (*DefineClass)(JNIEnv*, const char*, jobject, const jbyte*, jsize);
    jclass (*FindClass)(JNIEnv*, const char*);
    jmethodID (*FromReflectedMethod)(JNIEnv*, jobject);
    jfieldID (*FromReflectedField)(JNIEnv*, jobject);
    jobject (*ToReflectedMethod)(JNIEnv*, jclass, jmethodID, jboolean);
    jclass (*GetSuperclass)(JNIEnv*, jclass);
    jboolean (*IsAssignableFrom)(JNIEnv*, jclass, jclass);
    jobject (*ToReflectedField)(JNIEnv*, jclass, jfieldID, jboolean);

    jint (*Throw)(JNIEnv*, jthrowable);
    jint (*ThrowNew)(JNIEnv*, jclass, const char*);
    jthrowable (*ExceptionOccurred)(JNIEnv*);
    void (*ExceptionDescribe)(JNIEnv*);
    void (*ExceptionClear)(JNIEnv*);
    void (*FatalError)(JNIEnv*, const char*);

    jint (*PushLocalFrame)(JNIEnv*, jint);
    jobject (*PopLocalFrame)(JNIEnv*, jobject);

    jobject (*NewGlobalRef)(JNIEnv*, jobject);
    void (*DeleteGlobalRef)(JNIEnv*, jobject);
    void (*DeleteLocalRef)(JNIEnv*, jobject);
    jboolean (*IsSameObject)(JNIEnv*, jobject, jobject);
    jobject (*NewLocalRef)(JNIEnv*, jobject);
    jint (*EnsureLocalCapacity)(JNIEnv*, jint);

    jobject (*AllocObject)(JNIEnv*, jclass);
    jobject (*NewObject)(JNIEnv*, jclass, jmethodID, ...);
    jobject (*NewObjectA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jobject (*NewObjectV)(JNIEnv*, jclass, jmethodID, void*);

    jclass (*GetObjectClass)(JNIEnv*, jobject);
    jboolean (*IsInstanceOf)(JNIEnv*, jobject, jclass);
    jmethodID (*GetMethodID)(JNIEnv*, jclass, const char*, const char*);

    // CallObjectMethod
    jobject (*CallObjectMethod)(JNIEnv*, jobject, jmethodID, ...);
    jobject (*CallObjectMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jobject (*CallObjectMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jboolean (*CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);
    jboolean (*CallBooleanMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jboolean (*CallBooleanMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jbyte (*CallByteMethod)(JNIEnv*, jobject, jmethodID, ...);
    jbyte (*CallByteMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jbyte (*CallByteMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jchar (*CallCharMethod)(JNIEnv*, jobject, jmethodID, ...);
    jchar (*CallCharMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jchar (*CallCharMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jshort (*CallShortMethod)(JNIEnv*, jobject, jmethodID, ...);
    jshort (*CallShortMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jshort (*CallShortMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jint (*CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);
    jint (*CallIntMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jint (*CallIntMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jlong (*CallLongMethod)(JNIEnv*, jobject, jmethodID, ...);
    jlong (*CallLongMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jlong (*CallLongMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jfloat (*CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);
    jfloat (*CallFloatMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jfloat (*CallFloatMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jdouble (*CallDoubleMethod)(JNIEnv*, jobject, jmethodID, ...);
    jdouble (*CallDoubleMethodV)(JNIEnv*, jobject, jmethodID, void*);
    jdouble (*CallDoubleMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    void (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
    void (*CallVoidMethodV)(JNIEnv*, jobject, jmethodID, void*);
    void (*CallVoidMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    // [Padding table to reach GetDirectBufferAddress]
    void* padding[135];

    // Array operations
    jbyteArray (*NewByteArray)(JNIEnv*, jsize);
    jbyte* (*GetByteArrayElements)(JNIEnv*, jbyteArray, jboolean*);
    void (*ReleaseByteArrayElements)(JNIEnv*, jbyteArray, jbyte*, jint);
    void (*GetByteArrayRegion)(JNIEnv*, jbyteArray, jsize, jsize, jbyte*);
    void (*SetByteArrayRegion)(JNIEnv*, jbyteArray, jsize, jsize, const jbyte*);

    // Direct Buffers
    jobject (*NewDirectByteBuffer)(JNIEnv*, void*, jlong);
    void*   (*GetDirectBufferAddress)(JNIEnv*, jobject);
    jlong   (*GetDirectBufferCapacity)(JNIEnv*, jobject);
};
