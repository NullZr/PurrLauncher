#include "include/download.h"
#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>

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

    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;

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
    std::string filepath;

public:
    FileHandle(const std::string& path, const char* mode)
        : file(nullptr), filepath(path) {
        fopen_s(&file, path.c_str(), mode);
    }

    ~FileHandle() {
        if (file) {
            fclose(file);
        }
    }

    FILE* get() const { return file; }

    bool isValid() const { return file != nullptr; }

    void close() {
        if (file) {
            fclose(file);
            file = nullptr;
        }
    }

    const std::string& getPath() const { return filepath; }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};

// Progress tracking structure
struct ProgressData {
    std::chrono::steady_clock::time_point startTime;
    std::string filename;
    bool showProgress;
    curl_off_t lastDownloaded;
    std::chrono::steady_clock::time_point lastUpdate;

    ProgressData(const std::string& name, bool show = true)
        : startTime(std::chrono::steady_clock::now())
        , filename(name)
        , showProgress(show)
        , lastDownloaded(0)
        , lastUpdate(std::chrono::steady_clock::now()) {}
};

size_t write_data(const void* ptr, const size_t size, const size_t nmemb, FILE* stream) {
    if (!stream) {
        std::cerr << "\nError: File stream is null!" << std::endl;
        return 0;
    }
    if (!ptr) {
        std::cerr << "\nError: Data pointer is null!" << std::endl;
        return 0;
    }

    const size_t total_size = size * nmemb;
    const size_t written = fwrite(ptr, 1, total_size, stream);

    if (written != total_size) {
        std::cerr << "\nWarning: Write incomplete (expected " << total_size
                 << " bytes, wrote " << written << " bytes)" << std::endl;
    }

    return written;
}

int progress_func(void* ptr, const curl_off_t TotalToDownload, const curl_off_t NowDownloaded,
                 curl_off_t /*TotalToUpload*/, curl_off_t /*NowUploaded*/) {
    if (!ptr) return 0;

    auto* progressData = static_cast<ProgressData*>(ptr);
    if (!progressData->showProgress) return 0;

    // Update progress only every 250ms to reduce console spam
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - progressData->lastUpdate);

    if (timeSinceLastUpdate.count() < 250 && NowDownloaded > 0) {
        return 0;
    }

    progressData->lastUpdate = currentTime;

    // Check if this is a streaming download (no known total size)
    bool isStreaming = (TotalToDownload <= 0);

    if (isStreaming) {
        // Streaming mode - show spinner and downloaded size only
        const char spinner[] = {'|', '/', '-', '\\'};
        static int spinnerIndex = 0;
        spinnerIndex = (spinnerIndex + 1) % 4;

        std::cout << "\r[" << spinner[spinnerIndex] << "] Downloading";

        // Calculate download speed
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - progressData->startTime);
        if (elapsed.count() > 0 && NowDownloaded > 0) {
            double speedKBs = static_cast<double>(NowDownloaded) / elapsed.count() / 1024.0;

            // Show speed in appropriate units
            if (speedKBs > 1024.0) {
                std::cout << " (" << std::fixed << std::setprecision(2) << (speedKBs / 1024.0) << " MB/s)";
            } else {
                std::cout << " (" << std::fixed << std::setprecision(1) << speedKBs << " KB/s)";
            }
        }

        // Show downloaded size
        double downloadedMB = static_cast<double>(NowDownloaded) / (1024.0 * 1024.0);
        std::cout << " [" << std::fixed << std::setprecision(1) << downloadedMB << " MB]";
    } else {
        // Normal mode with known size - show progress bar
        constexpr int totaldotz = 40;
        const double fractiondownloaded = static_cast<double>(NowDownloaded) / static_cast<double>(TotalToDownload);
        const int dotz = static_cast<int>(fractiondownloaded * totaldotz);

        // Calculate download speed
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - progressData->startTime);

        std::cout << "\r[";
        for (int i = 0; i < dotz; ++i) std::cout << "=";
        for (int i = dotz; i < totaldotz; ++i) std::cout << " ";

        std::cout << "] " << static_cast<int>(fractiondownloaded * 100) << "%";

        // Show download speed and ETA
        if (elapsed.count() > 0 && NowDownloaded > 0) {
            double speedKBs = static_cast<double>(NowDownloaded) / elapsed.count() / 1024.0;

            if (speedKBs > 1024.0) {
                std::cout << " (" << std::fixed << std::setprecision(2) << (speedKBs / 1024.0) << " MB/s";
            } else {
                std::cout << " (" << std::fixed << std::setprecision(1) << speedKBs << " KB/s";
            }

            // Calculate ETA
            if (speedKBs > 0 && TotalToDownload > NowDownloaded) {
                double remainingBytes = TotalToDownload - NowDownloaded;
                double etaSeconds = remainingBytes / (speedKBs * 1024.0);
                if (etaSeconds < 3600) {
                    int minutes = static_cast<int>(etaSeconds / 60);
                    int seconds = static_cast<int>(etaSeconds) % 60;
                    std::cout << ", ETA: " << minutes << "m " << seconds << "s";
                }
            }
            std::cout << ")";
        }

        // Show downloaded size
        double downloadedMB = static_cast<double>(NowDownloaded) / (1024.0 * 1024.0);
        double totalMB = static_cast<double>(TotalToDownload) / (1024.0 * 1024.0);
        std::cout << " [" << std::fixed << std::setprecision(1) << downloadedMB << "/" << totalMB << " MB]";
    }

    std::cout << "        " << std::flush; // Extra spaces to clear previous text

    progressData->lastDownloaded = NowDownloaded;
    return 0;
}

// Verify file integrity after download
bool verifyDownloadedFile(const std::string& filepath, curl_off_t expectedSize, bool isStreaming) {
    if (!fs::exists(filepath)) {
        std::cerr << "Downloaded file does not exist: " << filepath << std::endl;
        return false;
    }

    try {
        auto actualSize = fs::file_size(filepath);

        if (actualSize == 0) {
            std::cerr << "Downloaded file is empty: " << filepath << std::endl;
            return false;
        }

        // For streaming downloads, we can't verify exact size, but check if it's reasonable
        if (isStreaming) {
            // Minimum size check - modpack should be at least 1MB
            if (actualSize < 1024 * 1024) {
                std::cerr << "Downloaded file seems too small: " << actualSize << " bytes" << std::endl;
                return false;
            }
            std::cout << "Downloaded file size: " << (actualSize / (1024.0 * 1024.0)) << " MB" << std::endl;
        } else {
            // If we know the expected size, verify it matches
            if (expectedSize > 0 && actualSize != static_cast<uintmax_t>(expectedSize)) {
                std::cerr << "Size mismatch: expected " << expectedSize
                         << " bytes, got " << actualSize << " bytes" << std::endl;
                return false;
            }
        }

        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to verify file: " << e.what() << std::endl;
        return false;
    }
}

// Main download function with retry logic
bool downloadFile(const std::string& url, const std::string& outputPath) {
    constexpr int MAX_RETRIES = 3;
    constexpr int RETRY_DELAY_SECONDS = 2;

    // Create parent directories if they don't exist
    if (const auto parent = fs::path(outputPath).parent_path(); !parent.empty()) {
        try {
            fs::create_directories(parent);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Failed to create directories: " << e.what() << std::endl;
            return false;
        }
    }

    // Extract filename for progress display
    std::string filename = fs::path(outputPath).filename().string();

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        if (attempt > 1) {
            std::cout << "\nRetry attempt " << attempt << "/" << MAX_RETRIES << "..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
        }

        // Remove partial file from previous failed attempt
        if (attempt > 1 && fs::exists(outputPath)) {
            try {
                fs::remove(outputPath);
                std::cout << "Removed incomplete file from previous attempt" << std::endl;
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Warning: Failed to remove incomplete file: " << e.what() << std::endl;
            }
        }

        CurlHandle curl;
        if (!curl.isValid()) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            if (attempt == MAX_RETRIES) return false;
            continue;
        }

        FileHandle file(outputPath, "wb");
        if (!file.isValid()) {
            std::cerr << "Failed to open file for writing: " << outputPath << std::endl;
            if (attempt == MAX_RETRIES) return false;
            continue;
        }

        ProgressData progressData(filename);

        // Configure CURL options
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file.get());
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_func);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &progressData);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file.get());
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_func);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &progressData);

        // Timeouts - adjusted for streaming downloads
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 120L);  // Allow 120s of slow speed for streaming
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 512L); // 512 bytes/s minimum (very low for streaming)

        // Enable compression
        curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");

        // Set user agent
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");

        // Performance optimizations
        curl_easy_setopt(curl.get(), CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_TCP_NODELAY, 1L); // Disable Nagle's algorithm for streaming
        curl_easy_setopt(curl.get(), CURLOPT_BUFFERSIZE, 524288L); // 512KB buffer for large files

        // SSL options
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

        // Support for chunked transfer encoding (streaming)
        curl_easy_setopt(curl.get(), CURLOPT_HTTP_TRANSFER_DECODING, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_HTTP_CONTENT_DECODING, 1L);

        // Enable resume support for interrupted downloads (not for first attempt)
        if (attempt > 1 && fs::exists(outputPath)) {
            try {
                auto fileSize = fs::file_size(outputPath);
                // Only resume if we have a significant partial download
                if (fileSize > 1024 * 1024) { // At least 1MB
                    curl_easy_setopt(curl.get(), CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(fileSize));
                    std::cout << "Resuming download from " << (fileSize / (1024.0 * 1024.0)) << " MB..." << std::endl;
                }
            } catch (...) {
                // Ignore resume errors
            }
        }

        std::cout << "Downloading: " << filename << std::endl;

        // Try to get file size with HEAD request (optional, don't fail if it doesn't work)
        if (attempt == 1) {
            CurlHandle headCurl;
            if (headCurl.isValid()) {
                curl_easy_setopt(headCurl.get(), CURLOPT_URL, url.c_str());
                curl_easy_setopt(headCurl.get(), CURLOPT_NOBODY, 1L);
                curl_easy_setopt(headCurl.get(), CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(headCurl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");
                curl_easy_setopt(headCurl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
                curl_easy_setopt(headCurl.get(), CURLOPT_TIMEOUT, 15L);

                CURLcode head_res = curl_easy_perform(headCurl.get());

                if (head_res == CURLE_OK) {
                    curl_off_t content_length = 0;
                    curl_easy_getinfo(headCurl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
                    if (content_length > 0) {
                        std::cout << "Expected file size: " << std::fixed << std::setprecision(2)
                                 << (content_length / (1024.0 * 1024.0)) << " MB" << std::endl;
                    } else {
                        std::cout << "Streaming download (size unknown)" << std::endl;
                    }
                }
            }
        }

        CURLcode res = curl_easy_perform(curl.get());

        // CRITICAL: Close and flush file BEFORE any verification
        file.close();

        // Give filesystem time to sync (especially on Windows)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << std::endl; // Newline after progress bar

        if (res != CURLE_OK) {
            std::cerr << "Download failed: " << curl_easy_strerror(res) << " (error code: " << res << ")" << std::endl;

            // Provide more context for common errors
            switch (res) {
                case CURLE_COULDNT_CONNECT:
                    std::cerr << "Could not connect to server. Check your internet connection." << std::endl;
                    break;
                case CURLE_OPERATION_TIMEDOUT:
                    std::cerr << "Connection timed out. Server might be slow or overloaded." << std::endl;
                    break;
                case CURLE_PARTIAL_FILE:
                    std::cerr << "Partial file transfer. Connection was interrupted." << std::endl;
                    break;
                case CURLE_WRITE_ERROR:
                    std::cerr << "Failed to write to disk. Check disk space and permissions." << std::endl;
                    break;
                case CURLE_RECV_ERROR:
                    std::cerr << "Failed to receive data. Connection issue." << std::endl;
                    break;
                default:
                    break;
            }

            // Clean up partial file
            if (fs::exists(outputPath)) {
                try {
                    auto partialSize = fs::file_size(outputPath);
                    std::cerr << "Partial download: " << (partialSize / (1024.0 * 1024.0)) << " MB" << std::endl;
                    fs::remove(outputPath);
                } catch (...) {}
            }

            if (attempt == MAX_RETRIES) {
                std::cerr << "Max retries reached. Download failed." << std::endl;
                return false;
            }
            continue;
        }

        // Check HTTP response code
        long response_code = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code >= 400) {
            std::cerr << "HTTP error " << response_code << " downloading " << url << std::endl;

            // Clean up partial file
            if (fs::exists(outputPath)) {
                try {
                    fs::remove(outputPath);
                } catch (...) {}
            }

            if (attempt == MAX_RETRIES) return false;
            continue;
        }

        // Verify downloaded file
        curl_off_t downloadSize = 0;
        curl_off_t contentLength = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_SIZE_DOWNLOAD_T, &downloadSize);
        curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);

        std::cout << "Downloaded: " << std::fixed << std::setprecision(2)
                 << (downloadSize / (1024.0 * 1024.0)) << " MB" << std::endl;

        // Check if file exists and has content
        if (!fs::exists(outputPath)) {
            std::cerr << "ERROR: File does not exist after download!" << std::endl;
            if (attempt == MAX_RETRIES) return false;
            continue;
        }

        try {
            auto actualFileSize = fs::file_size(outputPath);

            if (actualFileSize == 0) {
                std::cerr << "Downloaded file is empty (0 bytes)" << std::endl;
                if (fs::exists(outputPath)) {
                    try { fs::remove(outputPath); } catch (...) {}
                }
                if (attempt == MAX_RETRIES) return false;
                continue;
            }

            std::cout << "File on disk: " << std::fixed << std::setprecision(2)
                     << (actualFileSize / (1024.0 * 1024.0)) << " MB ("
                     << actualFileSize << " bytes)" << std::endl;

            // For small files (< 1KB), show content for debugging
            if (actualFileSize < 1024) {
                std::ifstream testFile(outputPath);
                std::string content((std::istreambuf_iterator<char>(testFile)),
                                   std::istreambuf_iterator<char>());
                std::cout << "File content preview: " << content.substr(0, 200) << std::endl;
            }

            // Success! File exists and has content
            std::cout << "✓ Download completed successfully: " << filename << std::endl;
            return true;

        } catch (const fs::filesystem_error& e) {
            std::cerr << "Could not verify file: " << e.what() << std::endl;
            if (attempt == MAX_RETRIES) return false;
            continue;
        } catch (const std::exception& e) {
            std::cerr << "Error reading file: " << e.what() << std::endl;
            if (attempt == MAX_RETRIES) return false;
            continue;
        }
    }

    return false;
}

static size_t write_callback(const char* ptr, const size_t size, const size_t nmemb, void* userdata) {
    auto* data = static_cast<std::string*>(userdata);
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string httpGet(const std::string& url) {
    constexpr int MAX_RETRIES = 3;

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        if (attempt > 1) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        CurlHandle curl;
        if (!curl.isValid()) {
            std::cerr << "Failed to initialize CURL for GET request" << std::endl;
            if (attempt == MAX_RETRIES) return "";
            continue;
        }

        std::string response;
        response.reserve(4096); // Pre-allocate more space

        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");
        curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode res = curl_easy_perform(curl.get());

        if (res != CURLE_OK) {
            std::cerr << "HTTP GET failed (attempt " << attempt << "): "
                     << curl_easy_strerror(res) << std::endl;
            if (attempt == MAX_RETRIES) return "";
            continue;
        }

        long http_code = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code < 200 || http_code >= 300) {
            std::cerr << "HTTP GET failed with code " << http_code
                     << " (attempt " << attempt << ")" << std::endl;
            if (attempt == MAX_RETRIES) return "";
            continue;
        }

        return response;
    }

    return "";
}

std::string httpPost(const std::string& url, const std::string& jsonData) {
    constexpr int MAX_RETRIES = 3;

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        if (attempt > 1) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        CurlHandle curl;
        if (!curl.isValid()) {
            std::cerr << "Failed to initialize CURL for POST request" << std::endl;
            if (attempt == MAX_RETRIES) return "";
            continue;
        }

        // RAII for curl_slist
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");

        auto cleanup_headers = [&headers]() {
            if (headers) {
                curl_slist_free_all(headers);
                headers = nullptr;
            }
        };

        std::string response;
        response.reserve(4096);

        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonData.length()));
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PurrLauncher/2.4.104");
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode res = curl_easy_perform(curl.get());
        cleanup_headers();

        if (res != CURLE_OK) {
            std::cerr << "HTTP POST failed (attempt " << attempt << "): "
                     << curl_easy_strerror(res) << std::endl;
            if (attempt == MAX_RETRIES) return "";
            continue;
        }

        long http_code = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code < 200 || http_code >= 300) {
            std::cerr << "HTTP POST failed with code " << http_code
                     << " (attempt " << attempt << ")" << std::endl;
            if (attempt == MAX_RETRIES) return "";
            continue;
        }

        return response;
    }

    return "";
}