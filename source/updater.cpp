#include "updater.hpp"
#include "downloader.hpp"
#include <curl/curl.h>
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace {
constexpr const char* CURRENT_VERSION = "1.3.2";
constexpr const char* RELEASE_API = "https://api.github.com/repos/XenoHzl/3DS-XPG/releases/latest";
constexpr const char* ASSET_NAME = "3DS_Eshop_XPG.nro";
constexpr const char* HELPER_NAME = "3DS_Eshop_XPG_Updater.nro";

bool copyFileStdio(const std::string& source, const std::string& destination) {
    FILE* input=fopen(source.c_str(),"rb"); if(!input) return false;
    FILE* output=fopen(destination.c_str(),"wb"); if(!output){fclose(input);return false;}
    char buffer[64*1024]; bool ok=true;
    while(true){const size_t n=fread(buffer,1,sizeof(buffer),input);if(n&&fwrite(buffer,1,n,output)!=n){ok=false;break;}if(n<sizeof(buffer)){if(ferror(input))ok=false;break;}}
    if(fflush(output)!=0) ok=false; fclose(output); fclose(input); return ok;
}

bool overwriteNative(const std::string& source, const std::string& destination, std::string& error) {
    FILE* input=fopen(source.c_str(),"rb"); if(!input){error="Cannot open downloaded update";return false;}
    fseek(input,0,SEEK_END); const long size=ftell(input); rewind(input);
    if(size<=0){fclose(input);error="Downloaded update is empty";return false;}
    FsFileSystem* filesystem=nullptr; char nativePath[FS_MAX_PATH]{};
    if(fsdevTranslatePath(destination.c_str(),&filesystem,nativePath)<0||!filesystem){fclose(input);error="Cannot translate NRO path";return false;}
    FsFile output{}; Result rc=fsFsOpenFile(filesystem,nativePath,FsOpenMode_Write,&output);
    if(R_FAILED(rc)){fclose(input);error="Cannot open current NRO with native FS";return false;}
    rc=fsFileSetSize(&output,size); char buffer[64*1024]; s64 offset=0;
    while(R_SUCCEEDED(rc)&&offset<size){const size_t want=(size-offset)<(long)sizeof(buffer)?(size_t)(size-offset):sizeof(buffer);const size_t n=fread(buffer,1,want,input);if(n!=want){rc=MAKERESULT(Module_Libnx,1);break;}rc=fsFileWrite(&output,offset,buffer,n,FsWriteOption_None);offset+=n;}
    if(R_SUCCEEDED(rc)) rc=fsFileFlush(&output); fsFileClose(&output); fclose(input);
    if(R_FAILED(rc)){error="Native FS write failed";return false;} return true;
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "3DS-Eshop-XPG-Updater/1.3.2");
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
    if (!info.version.empty() && !info.downloadUrl.empty()) info.available = newerThanCurrent(info.version);
    return info;
}

bool installUpdate(const UpdateInfo& info, const std::string& currentNroPath, std::string& error) {
    if (currentNroPath.empty() || currentNroPath.find(".nro") == std::string::npos) {
        error = "Cannot determine current NRO path";
        return false;
    }
    const std::string pending = currentNroPath + ".new";
    const std::string backup = currentNroPath + ".bak";
    remove(pending.c_str());
    if (!downloadFile(info.downloadUrl, pending, error)) return false;
    remove(backup.c_str());
    if(!copyFileStdio(currentNroPath,backup)){remove(pending.c_str());error="Cannot create update backup";return false;}
    if(!overwriteNative(pending,currentNroPath,error)){std::string restoreError;overwriteNative(backup,currentNroPath,restoreError);remove(pending.c_str());return false;}
    remove(pending.c_str());
    return true;
}
