#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace mlkit {

struct Point {
    int32_t x = 0;
    int32_t y = 0;
};

struct Barcode {
    int32_t format_id = 0;
    std::string format_name;
    std::string text;
    std::vector<uint8_t> raw_bytes;
    std::vector<Point> corners;
};

class MLKitScanner {
public:
    explicit MLKitScanner(const std::string& repo_root = "");
    ~MLKitScanner();

    // Prevent copy, allow move
    MLKitScanner(const MLKitScanner&) = delete;
    MLKitScanner& operator=(const MLKitScanner&) = delete;
    MLKitScanner(MLKitScanner&&) noexcept;
    MLKitScanner& operator=(MLKitScanner&&) noexcept;

    // Scan 8-bit RGBA image buffer
    std::vector<Barcode> scanRgba(const uint8_t* rgba_data, int width, int height, int stride = 0);

    // Scan 8-bit BGR image buffer (e.g. from cv::Mat)
    std::vector<Barcode> scanBgr(const uint8_t* bgr_data, int width, int height, int stride = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace mlkit
