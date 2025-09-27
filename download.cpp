// download.cpp
#include "include/download.h"
#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

// RAII wrapper for CURL handles
class CurlHandle {
private:
    CURL* curl;

public:
    CurlHandle() : curl(curl_easy_init()) {}

    ~CurlHandle() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    CURL* get() const { return curl; }

    bool isValid() const { return curl != nullptr; }

    // Disable copy constructor and assignment
    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;

    // Enable move semantics
    CurlHandle(CurlHandle&& other) noexcept : curl(other.curl) {
        other.curl = nullptr;
    }

    CurlHandle& operator=(CurlHandle&& other) noexcept {
        if (this != &other) {
            if (curl) curl_easy_cleanup(curl);
            curl = other.curl;
            other.curl = nullptr;
        }
        return *this;
    }
};

// RAII wrapper for FILE handles
class FileHandle {
private:
    FILE* file;

public:
    FileHandle(const std::string& path, const char* mode) : file(fopen(path.c_str(), mode)) {}

    ~FileHandle() {
        if (file) {
            fclose(file);
        }
    }

    FILE* get() const { return file; }

    bool isValid() const { return file != nullptr; }

    // Disable copy constructor and assignment
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};

// Progress tracking structure
struct ProgressData {
    std::chrono::steady_clock::time_point startTime;
    std::string filename;
    bool showProgress;

    ProgressData(const std::string& name, bool show = true)
        : startTime(std::chrono::steady_clock::now()), filename(name), showProgress(show) {}
};

size_t write_data(const void* ptr, const size_t size, const size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

int progress_func(void* ptr, const curl_off_t TotalToDownload, const curl_off_t NowDownloaded,
                 curl_off_t TotalToUpload, curl_off_t NowUploaded) {
    if (TotalToDownload <= 0 || !ptr) return 0;

    auto* progressData = static_cast<ProgressData*>(ptr);
    if (!progressData->showProgress) return 0;

    constexpr int totaldotz = 40;
    const double fractiondownloaded = static_cast<double>(NowDownloaded) / static_cast<double>(TotalToDownload);
    const int dotz = static_cast<int>(fractiondownloaded * totaldotz);

    // Calculate download speed
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - progressData->startTime);

    std::cout << "\r[";
    for (int i = 0; i < dotz; ++i) std::cout << "=";
    for (int i = dotz; i < totaldotz; ++i) std::cout << " ";

    std::cout << "] " << static_cast<int>(fractiondownloaded * 100) << "%";

    if (elapsed.count() > 0 && NowDownloaded > 0) {
        double speed = static_cast<double>(NowDownloaded) / elapsed.count() / 1024.0; // KB/s
        std::cout << " (" << std::fixed << std::setprecision(1) << speed << " KB/s)";
    }

    std::cout << std::flush;
    return 0;
}

bool downloadFile(const std::string& url, const std::string& outputPath) {
    // Create parent directories if they don't exist
    if (const auto parent = fs::path(outputPath).parent_path(); !parent.empty()) {
        try {
            fs::create_directories(parent);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Failed to create directories: " << e.what() << std::endl;
            return false;
        }
    }

    CurlHandle curl;
    if (!curl.isValid()) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    FileHandle file(outputPath, "wb");
    if (!file.isValid()) {
        std::cerr << "Failed to open file for writing: " << outputPath << std::endl;
        return false;
    }

    // Extract filename for progress display
    std::string filename = fs::path(outputPath).filename().string();
    ProgressData progressData(filename);

    // Configure CURL options for better performance and reliability
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file.get());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_func);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &progressData);

    // Set timeouts
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 300L); // 5 minutes total timeout

    // Enable compression
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");

    // Set user agent
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");

    // Performance optimizations
    curl_easy_setopt(curl.get(), CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_BUFFERSIZE, 102400L); // 100KB buffer

    CURLcode res = curl_easy_perform(curl.get());
    std::cout << std::endl; // Newline after progress bar

    if (res != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
        fs::remove(outputPath); // Clean up partial file
        return false;
    }

    // Check HTTP response code
    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 400) {
        std::cerr << "HTTP error " << response_code << " downloading " << url << std::endl;
        fs::remove(outputPath); // Clean up partial file
        return false;
    }

    return true;
}

static size_t write_callback(const char* ptr, const size_t size, const size_t nmemb, void* userdata) {
    auto* data = static_cast<std::string*>(userdata);
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string httpGet(const std::string& url) {
    CurlHandle curl;
    if (!curl.isValid()) {
        std::cerr << "Failed to initialize CURL for GET request" << std::endl;
        return "";
    }

    std::string response;
    response.reserve(1024); // Pre-allocate some space

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");

    CURLcode res = curl_easy_perform(curl.get());

    if (res != CURLE_OK) {
        std::cerr << "HTTP GET failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code < 200 || http_code >= 300) {
        std::cerr << "HTTP GET failed with code: " << http_code << " for URL: " << url << std::endl;
        return "";
    }

    return response;
}

std::string httpPost(const std::string& url, const std::string& jsonData) {
    CurlHandle curl;
    if (!curl.isValid()) {
        std::cerr << "Failed to initialize CURL for POST request" << std::endl;
        return "";
    }

    // RAII for curl_slist
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    // Automatic cleanup of headers
    auto cleanup_headers = [&headers]() {
        if (headers) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }
    };

    std::string response;
    response.reserve(1024);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonData.length()));
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");

    CURLcode res = curl_easy_perform(curl.get());
    cleanup_headers(); // Clean up headers

    if (res != CURLE_OK) {
        std::cerr << "HTTP POST failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code < 200 || http_code >= 300) {
        std::cerr << "HTTP POST failed with code: " << http_code << " for URL: " << url << std::endl;
        return "";
    }

    return response;
}