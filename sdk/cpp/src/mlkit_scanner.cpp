#include "mlkit_scanner.hpp"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <climits>
#include <map>

namespace mlkit {

// Mock Bitmap structure matching Android Bitmap representation in shim
struct MockBitmap {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t  format; // 1 = RGBA_8888
    void*    pixels;
};

// Protobuf parsing helpers
static inline uint64_t parseVarint(const uint8_t*& ptr, const uint8_t* end) {
    uint64_t res = 0;
    int shift = 0;
    while (ptr < end) {
        uint8_t b = *ptr++;
        res |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return res;
}

static Point decodePoint(const uint8_t* data, size_t len) {
    Point pt;
    const uint8_t* ptr = data;
    const uint8_t* end = data + len;
    while (ptr < end) {
        uint64_t tag = parseVarint(ptr, end);
        int field_num = tag >> 3;
        int wire_type = tag & 0x07;
        if (wire_type == 0) {
            uint64_t val = parseVarint(ptr, end);
            if (field_num == 1) pt.x = (int32_t)val;
            else if (field_num == 2) pt.y = (int32_t)val;
        } else if (wire_type == 2) {
            uint64_t length = parseVarint(ptr, end);
            ptr += length;
        } else {
            break;
        }
    }
    return pt;
}

static const std::map<int32_t, std::string> kFormatNames = {
    {1, "CODE_128"}, {2, "CODE_39"}, {4, "CODE_93"}, {8, "CODABAR"},
    {16, "DATA_MATRIX"}, {32, "EAN_13"}, {64, "EAN_8"}, {128, "ITF"},
    {256, "QR_CODE"}, {512, "UPC_A"}, {1024, "UPC_E"}, {2048, "PDF417"},
    {4096, "AZTEC"}, {32768, "TEZ_CODE"}
};

static const std::map<int32_t, int32_t> kInternalToStandard = {
    {1, 1}, {2, 2}, {3, 4}, {4, 8}, {5, 16}, {6, 32}, {7, 64}, {8, 128},
    {9, 256}, {10, 512}, {11, 1024}, {12, 2048}, {13, 4096}, {17, 32768}
};

static Barcode decodeBarcode(const uint8_t* data, size_t len) {
    Barcode b;
    const uint8_t* ptr = data;
    const uint8_t* end = data + len;
    while (ptr < end) {
        uint64_t tag = parseVarint(ptr, end);
        int field_num = tag >> 3;
        int wire_type = tag & 0x07;
        if (wire_type == 0) {
            uint64_t val = parseVarint(ptr, end);
            if (field_num == 1) {
                int32_t internal_id = (int32_t)val;
                auto it = kInternalToStandard.find(internal_id);
                b.format_id = (it != kInternalToStandard.end()) ? it->second : internal_id;
                auto name_it = kFormatNames.find(b.format_id);
                b.format_name = (name_it != kFormatNames.end()) ? name_it->second : ("FORMAT_" + std::to_string(internal_id));
            }
        } else if (wire_type == 2) {
            uint64_t length = parseVarint(ptr, end);
            if (ptr + length > end) break;
            if (field_num == 2) {
                b.raw_bytes.assign(ptr, ptr + length);
            } else if (field_num == 3) {
                b.text.assign((const char*)ptr, length);
            } else if (field_num == 11) {
                b.corners.push_back(decodePoint(ptr, length));
            }
            ptr += length;
        } else {
            break;
        }
    }
    return b;
}

static std::vector<Barcode> decodeResponse(const uint8_t* data, size_t len) {
    std::vector<Barcode> results;
    const uint8_t* ptr = data;
    const uint8_t* end = data + len;
    while (ptr < end) {
        uint64_t tag = parseVarint(ptr, end);
        int field_num = tag >> 3;
        int wire_type = tag & 0x07;
        if (wire_type == 2) {
            uint64_t length = parseVarint(ptr, end);
            if (ptr + length > end) break;
            if (field_num == 1) {
                results.push_back(decodeBarcode(ptr, length));
            }
            ptr += length;
        } else if (wire_type == 0) {
            parseVarint(ptr, end);
        } else {
            break;
        }
    }
    return results;
}

struct MLKitScanner::Impl {
    void* shim_handle = nullptr;
    void* barhopper_handle = nullptr;

    void* (*GetMockJNIEnv)() = nullptr;
    void* (*CreateMockByteArrayWithData)(const void*, int32_t) = nullptr;
    void (*FreeMockByteArray)(void*) = nullptr;
    void* (*GetLastProtobufResponse)(int32_t*) = nullptr;
    void (*HookBarhopperStdio)(void*) = nullptr;

    int64_t (*createNative)(void*, void*, void*) = nullptr;
    void (*closeNative)(void*, void*, int64_t) = nullptr;
    void* (*recognizeBitmapNative)(void*, void*, int64_t, void*, void*) = nullptr;

    void* jni_env = nullptr;
    int64_t barhopper_context = 0;
    void* mock_options_ptr = (void*)0x100;

    ~Impl() {
        if (barhopper_context && closeNative && jni_env) {
            closeNative(jni_env, nullptr, barhopper_context);
            barhopper_context = 0;
        }
        // Note: Do not dlclose to avoid running Bionic/glibc destructor collision
    }
};

MLKitScanner::MLKitScanner(const std::string& repo_root_in)
    : pimpl(std::make_unique<Impl>()) {

    std::string root = repo_root_in;
    if (root.empty()) {
        // Auto-detect based on current directory or environment
        const char* env_root = getenv("MLKIT_REPO_ROOT");
        if (env_root) {
            root = env_root;
        } else if (access("models/barhopper_options_official.bin", F_OK) == 0) {
            root = ".";
        } else if (access("../models/barhopper_options_official.bin", F_OK) == 0) {
            root = "..";
        } else if (access("../../models/barhopper_options_official.bin", F_OK) == 0) {
            root = "../..";
        } else {
            char exe_buf[PATH_MAX] = {0};
            if (readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1) > 0) {
                std::string exe_path = exe_buf;
                size_t pos = exe_path.rfind('/');
                if (pos != std::string::npos) {
                    std::string dir = exe_path.substr(0, pos);
                    if (access((dir + "/models/barhopper_options_official.bin").c_str(), F_OK) == 0) root = dir;
                    else if (access((dir + "/../models/barhopper_options_official.bin").c_str(), F_OK) == 0) root = dir + "/..";
                    else if (access((dir + "/../../models/barhopper_options_official.bin").c_str(), F_OK) == 0) root = dir + "/../..";
                }
            }
            if (root.empty()) {
                root = "/home/econsystems/econ/gmlqrkitport";
            }
        }
    }

    struct utsname uts;
    uname(&uts);
    std::string arch = uts.machine;
    std::string arch_dir = (arch == "aarch64" || arch == "arm64") ? "arm64-v8a" : "x86_64";

    std::string shim_path = root + "/core/lib/" + arch_dir + "/libandroid_shim.so";
    std::string barhopper_path = root + "/core/lib/" + arch_dir + "/libbarhopper_v3_patched.so";
    std::string options_path = root + "/models/barhopper_options_official.bin";

    pimpl->shim_handle = dlopen(shim_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!pimpl->shim_handle) {
        throw std::runtime_error("Failed to load shim library: " + std::string(dlerror()));
    }

    pimpl->barhopper_handle = dlopen(barhopper_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!pimpl->barhopper_handle) {
        throw std::runtime_error("Failed to load Barhopper library: " + std::string(dlerror()));
    }

    // Resolve shim functions
    pimpl->GetMockJNIEnv = (void* (*)())dlsym(pimpl->shim_handle, "GetMockJNIEnv");
    pimpl->CreateMockByteArrayWithData = (void* (*)(const void*, int32_t))dlsym(pimpl->shim_handle, "CreateMockByteArrayWithData");
    pimpl->FreeMockByteArray = (void (*)(void*))dlsym(pimpl->shim_handle, "FreeMockByteArray");
    pimpl->GetLastProtobufResponse = (void* (*)(int32_t*))dlsym(pimpl->shim_handle, "GetLastProtobufResponse");
    pimpl->HookBarhopperStdio = (void (*)(void*))dlsym(pimpl->shim_handle, "HookBarhopperStdio");

    if (pimpl->HookBarhopperStdio) {
        pimpl->HookBarhopperStdio(pimpl->barhopper_handle);
    }

    // Resolve Barhopper functions
    pimpl->createNative = (int64_t (*)(void*, void*, void*))dlsym(
        pimpl->barhopper_handle, "Java_com_google_android_libraries_barhopper_BarhopperV3_createNativeWithClientOptions");
    pimpl->closeNative = (void (*)(void*, void*, int64_t))dlsym(
        pimpl->barhopper_handle, "Java_com_google_android_libraries_barhopper_BarhopperV3_closeNative");
    pimpl->recognizeBitmapNative = (void* (*)(void*, void*, int64_t, void*, void*))dlsym(
        pimpl->barhopper_handle, "Java_com_google_android_libraries_barhopper_BarhopperV3_recognizeBitmapNative");

    if (!pimpl->createNative || !pimpl->recognizeBitmapNative || !pimpl->closeNative) {
        throw std::runtime_error("Failed to resolve Barhopper native symbols");
    }

    pimpl->jni_env = pimpl->GetMockJNIEnv();

    // Read options protobuf
    std::ifstream file(options_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open options file: " + options_path);
    }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> opt_buffer(size);
    file.read((char*)opt_buffer.data(), size);

    void* mock_arr = pimpl->CreateMockByteArrayWithData(opt_buffer.data(), (int32_t)size);
    pimpl->barhopper_context = pimpl->createNative(pimpl->jni_env, nullptr, mock_arr);
    pimpl->FreeMockByteArray(mock_arr);

    if (pimpl->barhopper_context == 0) {
        throw std::runtime_error("Failed to initialize Google Barhopper V3 engine context");
    }
}

MLKitScanner::~MLKitScanner() = default;
MLKitScanner::MLKitScanner(MLKitScanner&&) noexcept = default;
MLKitScanner& MLKitScanner::operator=(MLKitScanner&&) noexcept = default;

std::vector<Barcode> MLKitScanner::scanRgba(const uint8_t* rgba_data, int width, int height, int stride) {
    if (!rgba_data || width <= 0 || height <= 0) return {};
    if (stride <= 0) stride = width * 4;

    MockBitmap bm;
    bm.width = (uint32_t)width;
    bm.height = (uint32_t)height;
    bm.stride = (uint32_t)stride;
    bm.format = 1; // RGBA_8888
    bm.pixels = const_cast<uint8_t*>(rgba_data);

    pimpl->recognizeBitmapNative(pimpl->jni_env, nullptr, pimpl->barhopper_context, &bm, pimpl->mock_options_ptr);

    int32_t out_len = 0;
    const uint8_t* proto_data = (const uint8_t*)pimpl->GetLastProtobufResponse(&out_len);
    if (proto_data && out_len > 0) {
        return decodeResponse(proto_data, (size_t)out_len);
    }
    return {};
}

std::vector<Barcode> MLKitScanner::scanBgr(const uint8_t* bgr_data, int width, int height, int stride) {
    if (!bgr_data || width <= 0 || height <= 0) return {};
    if (stride <= 0) stride = width * 3;

    // Convert BGR to RGBA
    std::vector<uint8_t> rgba(width * height * 4);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row_in = bgr_data + y * stride;
        uint8_t* row_out = rgba.data() + y * (width * 4);
        for (int x = 0; x < width; ++x) {
            row_out[x * 4 + 0] = row_in[x * 3 + 2]; // R
            row_out[x * 4 + 1] = row_in[x * 3 + 1]; // G
            row_out[x * 4 + 2] = row_in[x * 3 + 0]; // B
            row_out[x * 4 + 3] = 255;              // A
        }
    }
    return scanRgba(rgba.data(), width, height, width * 4);
}

} // namespace mlkit
