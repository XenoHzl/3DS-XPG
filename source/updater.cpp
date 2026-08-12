#include "updater.hpp"
#include "downloader.hpp"
#include <curl/curl.h>
extern "C" {
#include <switch/runtime/env.h>
}
#include <cstdio>
#include <cstdlib>

namespace {
constexpr const char* CURRENT_VERSION = "1.2.4";
constexpr const char* RELEASE_API = "https://api.github.com/repos/XenoHzl/3DS-XPG/releases/latest";
constexpr const char* ASSET_NAME = "3DS_Eshop_XPG.nro";
constexpr const char* HELPER_NAME = "3DS_Eshop_XPG_Updater.nro";

bool copyFile(const std::string& source, const std::string& destination) {
    FILE* input = fopen(source.c_str(), "rb");
    if (!input) return false;
    FILE* output = fopen(destination.c_str(), "wb");
    if (!output) { fclose(input); return false; }
    char buffer[64 * 1024];
    bool ok = true;
    while (true) {
        const size_t count = fread(buffer, 1, sizeof(buffer), input);
        if (count && fwrite(buffer, 1, count, output) != count) { ok = false; break; }
        if (count < sizeof(buffer)) { if (ferror(input)) ok = false; break; }
    }
    if (fflush(output) != 0) ok = false;
    fclose(output);
    fclose(input);
    return ok;
}

size_t memoryWrite(char* data, size_t size, size_t count, void* userdata) {
    auto* output = static_cast<std::string*>(userdata);
    output->append(data, size * count);
    return size * count;
}

std::string jsonString(const std::string& json, const std::string& key, std::size_t start = 0) {
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker, start);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + marker.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    const std::size_t end = json.find('"', pos + 1);
    return end == std::string::npos ? std::string{} : json.substr(pos + 1, end - pos - 1);
}

void parseVersion(const std::string& value, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    const char* p = value.c_str();
    if (*p == 'v' || *p == 'V') ++p;
    for (int i = 0; i < 3 && *p; ++i) {
        out[i] = std::strtol(p, const_cast<char**>(&p), 10);
        if (*p == '.') ++p;
    }
}

bool newerThanCurrent(const std::string& remote) {
    int a[3], b[3];
    parseVersion(remote, a); parseVersion(CURRENT_VERSION, b);
    for (int i = 0; i < 3; ++i) {
        if (a[i] != b[i]) return a[i] > b[i];
    }
    return false;
}
}

UpdateInfo checkForUpdate() {
    UpdateInfo info;
    std::string json;
    CURL* curl = curl_easy_init();
    if (!curl) return info;
    curl_easy_setopt(curl, CURLOPT_URL, RELEASE_API);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "3DS-Eshop-XPG-Updater/1.2.4");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memoryWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status == 404) return info;
    if (status != 200) { info.error = "GitHub update check failed"; return info; }

    info.version = jsonString(json, "tag_name");
    std::size_t asset = json.find(std::string("\"name\":\"") + ASSET_NAME + "\"");
    if (asset == std::string::npos) asset = json.find(ASSET_NAME);
    if (asset != std::string::npos) info.downloadUrl = jsonString(json, "browser_download_url", asset);
    std::size_t helper = json.find(std::string("\"name\":\"") + HELPER_NAME + "\"");
    if (helper == std::string::npos) helper = json.find(HELPER_NAME);
    if (helper != std::string::npos) info.helperUrl = jsonString(json, "browser_download_url", helper);
    if (!info.version.empty() && !info.downloadUrl.empty() && !info.helperUrl.empty()) info.available = newerThanCurrent(info.version);
    return info;
}

bool installUpdate(const UpdateInfo& info, const std::string& currentNroPath, std::string& error) {
    if (currentNroPath.empty() || currentNroPath.find(".nro") == std::string::npos) {
        error = "Cannot determine current NRO path";
        return false;
    }
    const std::string pending = currentNroPath + ".new";
    const std::string helper = "sdmc:/switch/3DS_Eshop_XPG/3DS_Eshop_XPG_Updater.nro";
    const std::string target = "sdmc:/switch/3DS_Eshop_XPG/update_target.txt";
    remove(pending.c_str());
    if (!downloadFile(info.downloadUrl, pending, error)) return false;
    if (!downloadFile(info.helperUrl, helper, error)) { remove(pending.c_str()); return false; }
    FILE* targetFile = fopen(target.c_str(), "wb");
    if (!targetFile) { remove(pending.c_str()); error = "Cannot prepare update target"; return false; }
    fwrite(currentNroPath.data(), 1, currentNroPath.size(), targetFile);
    fclose(targetFile);
    envSetNextLoad(helper.c_str(), helper.c_str());
    return true;
}
