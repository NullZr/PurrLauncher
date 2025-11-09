#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <string>

// Download file from URL to local path with retry support
// Supports both regular and streaming downloads
bool downloadFile(const std::string& url, const std::string& outputPath);

// Perform HTTP GET request
std::string httpGet(const std::string& url);

// Perform HTTP POST request with JSON data
std::string httpPost(const std::string& url, const std::string& jsonData);

#endif // DOWNLOAD_H