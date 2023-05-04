#include "file_utils.h"

#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <iomanip>

TNoDirError::TNoDirError(const std::string& msg): std::runtime_error(msg)
{}

bool TryOpen(const std::vector<std::string>& fnames, std::ifstream& file)
{
    for (auto& fname: fnames) {
        file.open(fname);
        if (file.is_open()) {
            return true;
        }
        file.clear();
    }
    return false;
}

void WriteToFile(const std::string& fileName, const std::string& value)
{
    std::ofstream f;
    OpenWithException(f, fileName);
    f << value;
}

void IterateDir(const std::string& dirName, std::function<bool(const std::string&)> fn)
{
    try {
        const std::filesystem::path dirPath{dirName};

        for (const auto& entry: std::filesystem::directory_iterator(dirPath)) {
            const auto filenameStr = entry.path().filename().string();
            if (fn(filenameStr)) {
                return;
            }
        }
    } catch (std::filesystem::filesystem_error const& ex) {
        throw TNoDirError(ex.what());
    }
}
