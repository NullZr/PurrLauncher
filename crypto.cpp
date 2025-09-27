#include "include/crypto.h"
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include <iphlpapi.h>  // Для MAC
#pragma comment(lib, "IPHLPAPI.lib")

std::vector<unsigned char> computeMD5(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::vector<unsigned char> hash(16);

    CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);
    CryptHashData(hHash, reinterpret_cast<const BYTE*>(input.c_str()), static_cast<DWORD>(input.length()), 0);
    DWORD hashLen = 16;
    CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0);

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return hash;
}

std::string generateOfflineUUID(const std::string& username) {
    std::string input = "OfflinePlayer:" + username;
    auto md5Bytes = computeMD5(input);
    if (md5Bytes.empty()) return "";

    md5Bytes[6] = (md5Bytes[6] & 0x0F) | 0x30;  // Version 3
    md5Bytes[8] = (md5Bytes[8] & 0x3F) | 0x80;  // Variant 1

    std::stringstream ss;
    for (size_t i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md5Bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) ss << "-";
    }
    return ss.str();
}

std::string getHWID() {
    std::string hwid_raw;

    // Получить MAC-адрес
    PIP_ADAPTER_INFO pAdapterInfo = nullptr;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = static_cast<IP_ADAPTER_INFO *>(HeapAlloc(GetProcessHeap(), 0, ulOutBufLen));
    if (pAdapterInfo == nullptr) return "ERROR_HEAP_ALLOC_MAC";  // Ошибка аллокации

    DWORD ret = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        HeapFree(GetProcessHeap(), 0, pAdapterInfo);
        pAdapterInfo = static_cast<IP_ADAPTER_INFO *>(HeapAlloc(GetProcessHeap(), 0, ulOutBufLen));
        if (pAdapterInfo == nullptr) return "ERROR_HEAP_REALLOC_MAC";
        ret = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    }

    if (ret == NO_ERROR) {
        bool found = false;
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET) {
                for (UINT i = 0; i < pAdapter->AddressLength; i++) {
                    char buf[3];
                    sprintf_s(buf, "%.2X", (int)pAdapter->Address[i]);
                    hwid_raw += buf;
                }
                found = true;
                break;
            }
            pAdapter = pAdapter->Next;
        }
        if (!found) {
            HeapFree(GetProcessHeap(), 0, pAdapterInfo);
            return "NO_ETHERNET_ADAPTER";  // Нет Ethernet-адаптера
        }
    } else {
        HeapFree(GetProcessHeap(), 0, pAdapterInfo);
        return "ERROR_GET_ADAPTERS_" + std::to_string(ret);  // Ошибка API
    }
    HeapFree(GetProcessHeap(), 0, pAdapterInfo);

    // Получить серийный номер диска C:
    DWORD volumeSerial = 0;
    if ([[maybe_unused]] BOOL volRet = GetVolumeInformationA("C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, nullptr, 0)) {
        hwid_raw += std::to_string(volumeSerial);
    } else {
        return "ERROR_GET_VOLUME_" + std::to_string(GetLastError());
    }

    // Хэшировать MD5
    const auto md5Bytes = computeMD5(hwid_raw);
    std::stringstream ss;
    for (const auto byte : md5Bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}