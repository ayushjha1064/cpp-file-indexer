#pragma once

#include "index.h"

#include <string>

std::string persistentIndexPath(const std::string& directory,
                                const std::string& versionName);
void saveVersionAtomically(const std::string& directory,
                           const std::string& versionName,
                           const WordIndex& index);
WordIndex loadVersionFromDisk(const std::string& directory,
                              const std::string& versionName);
