// download.h
#pragma once
#include <string>
#include <filesystem>
#include <curl/curl.h>

namespace fs = std::filesystem;

size_t write_data(const void* ptr, const size_t size, const size_t nmemb, FILE* stream);
int progress_func(void* ptr, const curl_off_t TotalToDownload, const curl_off_t NowDownloaded, curl_off_t TotalToUpload, curl_off_t NowUploaded);
bool downloadFile(const std::string& url, const std::string& outputPath);
static size_t write_callback(const char* ptr, const size_t size, const size_t nmemb, void* userdata);
std::string httpGet(const std::string& url);
std::string httpPost(const std::string& url, const std::string& jsonData);