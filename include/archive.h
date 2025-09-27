#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <string>

bool extractArchive(const std::string& zipPath, const std::string& extractDir);

#endif // ARCHIVE_H