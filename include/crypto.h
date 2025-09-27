#ifndef CRYPTO_H
#define CRYPTO_H

#include <vector>
#include <string>

std::vector<unsigned char> computeMD5(const std::string& input);
std::string generateOfflineUUID(const std::string& username);
std::string getHWID();  // Новая функция для HWID

#endif