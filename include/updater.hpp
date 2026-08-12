#pragma once
#include <string>

struct UpdateInfo {
    bool available = false;
    std::string version;
    std::string downloadUrl;
    std::string helperUrl;
    std::string error;
};

UpdateInfo checkForUpdate();
bool installUpdate(const UpdateInfo& info, const std::string& currentNroPath, std::string& error);
