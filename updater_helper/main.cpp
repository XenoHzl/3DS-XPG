#include <switch.h>
extern "C" {
#include <switch/runtime/env.h>
}
#include <cstdio>
#include <string>

namespace {
constexpr const char* TARGET_FILE = "sdmc:/switch/3DS_Eshop_XPG/update_target.txt";
constexpr const char* LOG_FILE = "sdmc:/switch/3DS_Eshop_XPG/update.log";

void logResult(const char* message) {
    FILE* log = fopen(LOG_FILE, "wb");
    if (log) { fputs(message, log); fclose(log); }
}

std::string readTarget() {
    FILE* file = fopen(TARGET_FILE, "rb");
    if (!file) return {};
    char path[1024]{};
    const size_t count = fread(path, 1, sizeof(path) - 1, file);
    fclose(file);
    return std::string(path, count);
}

void waitForChoice(const std::string& target, bool success) {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    printf("\x1b[2J\x1b[1;1H3DS Eshop XPG Updater\n\n");
    if (success) printf("Update Success\n\n[A] Reboot\n[B] Return to Homebrew Menu\n");
    else printf("Update Failed\n\nCheck:\n%s\n\n[B] Return to Homebrew Menu\n", LOG_FILE);
    consoleUpdate(nullptr);
    while (appletMainLoop()) {
        padUpdate(&pad); const u64 keys = padGetButtonsDown(&pad);
        if (success && (keys & HidNpadButton_A)) { envSetNextLoad(target.c_str(), target.c_str()); break; }
        if (keys & HidNpadButton_B) break;
        svcSleepThread(20000000ULL);
    }
    consoleExit(nullptr);
}
}

int main() {
    const std::string target = readTarget();
    if (target.empty()) { logResult("Cannot read update target"); waitForChoice(target, false); return 0; }
    const std::string pending = target + ".new", backup = target + ".bak";
    svcSleepThread(500000000ULL); remove(backup.c_str());
    if (rename(target.c_str(), backup.c_str()) != 0) { logResult("Cannot rename current NRO to backup"); waitForChoice(target, false); return 0; }
    if (rename(pending.c_str(), target.c_str()) != 0) { rename(backup.c_str(), target.c_str()); logResult("Cannot rename pending NRO to current NRO"); waitForChoice(target, false); return 0; }
    remove(TARGET_FILE); logResult("Update Success"); waitForChoice(target, true); return 0;
}
