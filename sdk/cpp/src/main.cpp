#include "mlkit_scanner.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <unistd.h>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

// Lightweight uncompressed BMP loader for zero-dependency execution
static bool loadBmp(const std::string& path, std::vector<uint8_t>& bgr, int& width, int& height) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    uint8_t header[54];
    f.read(reinterpret_cast<char*>(header), 54);
    if (f.gcount() < 54 || header[0] != 'B' || header[1] != 'M') return false;

    width = *reinterpret_cast<int32_t*>(&header[18]);
    height = *reinterpret_cast<int32_t*>(&header[22]);
    uint16_t bpp = *reinterpret_cast<uint16_t*>(&header[28]);
    uint32_t offset = *reinterpret_cast<uint32_t*>(&header[10]);

    if (bpp != 24 && bpp != 32) return false;

    bool flip_vertically = (height > 0);
    height = std::abs(height);

    f.seekg(offset, std::ios::beg);
    int row_stride = ((width * (bpp / 8) + 3) / 4) * 4;
    std::vector<uint8_t> raw_row(row_stride);

    bgr.resize(width * height * 3);

    for (int y = 0; y < height; ++y) {
        f.read(reinterpret_cast<char*>(raw_row.data()), row_stride);
        int target_y = flip_vertically ? (height - 1 - y) : y;
        uint8_t* dst = bgr.data() + target_y * width * 3;

        for (int x = 0; x < width; ++x) {
            dst[x * 3 + 0] = raw_row[x * (bpp / 8) + 0]; // B
            dst[x * 3 + 1] = raw_row[x * (bpp / 8) + 1]; // G
            dst[x * 3 + 2] = raw_row[x * (bpp / 8) + 2]; // R
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Google ML Kit Barcode Scanner C++ Native Runner ===" << std::endl;

    try {
        mlkit::MLKitScanner scanner;

        std::vector<std::string> test_files = {
            "samples/sample_qr.bmp",
            "samples/sample_code128.bmp",
            "samples/sample_qr_rotated.bmp"
        };

        if (argc > 1) {
            test_files.clear();
            for (int i = 1; i < argc; ++i) {
                test_files.push_back(argv[i]);
            }
        }

        for (const auto& file : test_files) {
            int w = 0, h = 0;
            std::vector<uint8_t> bgr;

            bool loaded = false;
#ifdef HAVE_OPENCV
            cv::Mat mat = cv::imread(file);
            if (!mat.empty()) {
                w = mat.cols;
                h = mat.rows;
                bgr.assign(mat.data, mat.data + (w * h * 3));
                loaded = true;
            }
#endif
            if (!loaded) {
                loaded = loadBmp(file, bgr, w, h);
            }

            if (!loaded) {
                std::cout << "[!] Could not load or unsupported file: " << file << std::endl;
                continue;
            }

            auto start = std::chrono::high_resolution_clock::now();
            auto results = scanner.scanBgr(bgr.data(), w, h);
            auto end = std::chrono::high_resolution_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();

            std::cout << "\n[*] Results for " << file << " (" << w << "x" << h << ", Latency: " << latency_ms << " ms):" << std::endl;
            for (const auto& b : results) {
                std::cout << "    Format:  " << b.format_name << " (ID: " << b.format_id << ")" << std::endl;
                std::cout << "    Text:    " << b.text << std::endl;
                std::cout << "    Corners: ";
                for (const auto& pt : b.corners) {
                    std::cout << "(" << pt.x << ", " << pt.y << ") ";
                }
                std::cout << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n=== Scan Complete ===" << std::endl;
    // Fast exit to prevent Bionic/glibc static destructor collision
    _exit(0);
}
