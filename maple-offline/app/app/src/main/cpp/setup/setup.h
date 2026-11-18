// maple-offline first-run asset setup: installer extraction and WZ -> NX conversion.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <algorithm>

namespace msetup {
// stage: "extract" / "convert"; done/total in bytes (convert: files)
using Progress = std::function<void(const char *stage, uint64_t done, uint64_t total)>;

// Extract every file of the GMS v83 installer's embedded cabinet set into outDir.
bool extract_installer(const std::string &setupExe, const std::string &outDir,
                       const Progress &progress, std::string &err);
}
