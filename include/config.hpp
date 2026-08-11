#pragma once
#include <cstddef>

namespace Config {

struct DownloadItem {
    const char* title;
    const char* description;
    const char* url;
    const char* fileName;
    const char* coverPath;
    const char* mirrorUrl;
    const char* destinationPath = "sdmc:/roms/3ds/";
    bool extractZip = false;
};

// Add only files you own or are authorized to redistribute.
// Set destinationPath per item. Set extractZip=true only for ZIP archives.
constexpr DownloadItem DOWNLOADS[] = {
    {
        "Example homebrew package",
        "Replace this entry with your authorized download",
        "https://example.com/homebrew.zip",
        "homebrew.zip",
        "sdmc:/switch/3DS_Eshop_XPG/images/example.png",
        nullptr,
        "sdmc:/switch/3DS_Eshop_XPG/downloads/",
        true
    }
};

constexpr std::size_t DOWNLOAD_COUNT = sizeof(DOWNLOADS) / sizeof(DOWNLOADS[0]);
constexpr const char* TEMP_DIR = "sdmc:/switch/3DS_Eshop_XPG";
constexpr const char* DEST_DIR = "sdmc:/roms/3ds/";

}
