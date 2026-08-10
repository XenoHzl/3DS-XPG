#include "downloader.hpp"
#include "ui.hpp"
#include <switch.h>
#include <curl/curl.h>
#include <cstdio>

namespace {
struct Context {
    bool cancel = false;
    PadState pad;
    u64 started = 0;
    u64 lastUiUpdate = 0;
};

size_t writeCb(char* data, size_t size, size_t count, void* file) {
    return fwrite(data, size, count, static_cast<FILE*>(file));
}

int progressCb(void* userdata, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<Context*>(userdata);
    padUpdate(&ctx->pad);
    if (padGetButtonsDown(&ctx->pad) & HidNpadButton_B) {
        ctx->cancel = true;
        return 1;
    }

    // Console rendering is relatively expensive on Switch. Updating for every
    // received block can throttle the transfer, so redraw at most four times
    // per second (and always draw the final state).
    const u64 currentTick = armGetSystemTick();
    const u64 uiInterval = 19200000ULL / 4;
    if (now != total && currentTick - ctx->lastUiUpdate < uiInterval) return 0;
    ctx->lastUiUpdate = currentTick;

    const double elapsed = static_cast<double>(currentTick - ctx->started) / 19200000.0;
    const double speed = elapsed > 0.0 ? static_cast<double>(now) / elapsed : 0.0;
    const int pct = total > 0 ? static_cast<int>((100 * now) / total) : 0;
    const double nowMb = static_cast<double>(now) / (1024.0 * 1024.0);
    const double totalMb = static_cast<double>(total) / (1024.0 * 1024.0);
    const double speedMb = speed / (1024.0 * 1024.0);
    uiRenderDownloadProgress(pct, nowMb, totalMb, speedMb);
    return 0;
}
}

bool downloadFile(const std::string& url, const std::string& output, std::string& error) {
    CURL* curl = curl_easy_init();
    if (!curl) { error = "Unable to initialize network download"; return false; }

    const std::string partial = output + ".part";
    const std::string backup = output + ".old";
    remove(partial.c_str());
    FILE* file = fopen(partial.c_str(), "wb");
    if (!file) { curl_easy_cleanup(curl); error = "Cannot create download file on SD card"; return false; }
    setvbuf(file, nullptr, _IOFBF, 1024 * 1024);

    Context ctx;
    padInitializeDefault(&ctx.pad);
    ctx.started = armGetSystemTick();
    ctx.lastUiUpdate = ctx.started - (19200000ULL / 4);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 3DS-Eshop-XPG/1.2");

    const CURLcode result = curl_easy_perform(curl);
    char* responseTypeRaw = nullptr;
    double downloadedBytes = 0.0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &responseTypeRaw);
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloadedBytes);
    const std::string responseType = responseTypeRaw ? responseTypeRaw : "";
    fclose(file);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        remove(partial.c_str());
        error = ctx.cancel ? "Download cancelled" : curl_easy_strerror(result);
        return false;
    }
    if (downloadedBytes <= 0.0) {
        remove(partial.c_str());
        error = "Server returned an empty file";
        return false;
    }
    if (responseType.find("text/html") != std::string::npos) {
        remove(partial.c_str());
        error = "网络繁忙，请稍后再试";
        return false;
    }

    // Replace only after the new file has downloaded completely. If anything
    // fails, restore the original file instead of leaving a broken download.
    remove(backup.c_str());
    const bool hadExistingFile = rename(output.c_str(), backup.c_str()) == 0;
    if (rename(partial.c_str(), output.c_str()) != 0) {
        if (hadExistingFile) rename(backup.c_str(), output.c_str());
        remove(partial.c_str());
        error = "Cannot replace existing file";
        return false;
    }
    if (hadExistingFile) remove(backup.c_str());
    return true;
}
