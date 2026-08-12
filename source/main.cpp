#include "config.hpp"
#include "downloader.hpp"
#include "extractor.hpp"
#include "ui.hpp"
#include "updater.hpp"

#include <switch.h>
#include <curl/curl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>

namespace {
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
TTF_Font* font = nullptr;
TTF_Font* fontSmall = nullptr;
PadState pad;
SDL_Texture* covers[Config::DOWNLOAD_COUNT]{};
SDL_Texture* backgroundTexture = nullptr;
Mix_Music* backgroundMusic = nullptr;

const SDL_Color WHITE{240,240,245,255};
const SDL_Color GREY{155,160,170,255};
const SDL_Color BLUE{55,95,255,255};
const SDL_Color GREEN{40,210,125,255};
const SDL_Color RED{245,75,90,255};

void ensureDirectory(const std::string& path) {
    std::string current;
    for (char ch : path) {
        current += ch;
        if (ch == '/' && current.size() > 6) mkdir(current.c_str(), 0777);
    }
    if (!current.empty() && current.back() != '/') mkdir(current.c_str(), 0777);
}

void text(const std::string& value, int x, int y, SDL_Color color, TTF_Font* useFont = nullptr) {
    if (value.empty()) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(useFont ? useFont : font, value.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void frame(int x, int y, int w, int h, SDL_Color color, int thickness = 2) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i=0; i<thickness; ++i) { SDL_Rect r{x+i,y+i,w-i*2,h-i*2}; SDL_RenderDrawRect(renderer,&r); }
}

void begin(const char* title) {
    SDL_SetRenderDrawColor(renderer, 7, 9, 15, 255);
    SDL_RenderClear(renderer);
    if(backgroundTexture) {
        SDL_Rect screen{0,0,1280,720};
        SDL_RenderCopy(renderer,backgroundTexture,nullptr,&screen);
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,0,0,0,150);
        SDL_RenderFillRect(renderer,&screen);
    }
    SDL_SetRenderDrawColor(renderer, 28, 32, 44, 255);
    SDL_Rect top{0,0,1280,68}; SDL_RenderFillRect(renderer,&top);
    text("XPerfect 3DS", 26, 17, WHITE);
    text(title, 470, 17, WHITE);
    frame(10,10,1260,700,{100,108,125,255},1);
}

void footer(const char* controls) {
    SDL_SetRenderDrawColor(renderer, 28,32,44,255);
    SDL_Rect bar{11,660,1258,49}; SDL_RenderFillRect(renderer,&bar);
    text(controls, 28, 671, WHITE, fontSmall);
}

void icon(int x, int y, int w, int h, std::size_t index) {
    if (index < Config::DOWNLOAD_COUNT && covers[index]) {
        SDL_Rect dst{x,y,w,h};
        SDL_RenderCopy(renderer,covers[index],nullptr,&dst);
        return;
    }
    const Uint8 colors[][3]={{58,55,150},{135,45,100},{28,120,125},{150,85,30}};
    const auto& c=colors[index%4];
    SDL_SetRenderDrawColor(renderer,c[0],c[1],c[2],255);
    SDL_Rect bg{x,y,w,h}; SDL_RenderFillRect(renderer,&bg);
    filledCircleRGBA(renderer,x+w/2,y+h/2,48,235,235,245,255);
    filledCircleRGBA(renderer,x+w/2,y+h/2,39,c[0],c[1],c[2],255);
    thickLineRGBA(renderer,x+w/2-48,y+h/2,x+w/2+48,y+h/2,235,235,245,255,8);
    filledCircleRGBA(renderer,x+w/2,y+h/2,13,235,235,245,255);
}

void showMessage(const char* heading, const std::string& message, SDL_Color color) {
    begin(heading);
    frame(215,190,850,300,{70,78,98,255},2);
    filledCircleRGBA(renderer,640,275,42,color.r,color.g,color.b,255);
    text(message,300,350,WHITE);
    footer("B  Back      +  Exit");
    SDL_RenderPresent(renderer);
}

bool initUi() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0 || TTF_Init() != 0) return false;
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    window=SDL_CreateWindow("XPerfect 3DS",0,0,1280,720,SDL_WINDOW_FULLSCREEN);
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) return false;
    if (R_FAILED(plInitialize(PlServiceType_User))) return false;
    PlFontData data{};
    if (R_FAILED(plGetSharedFontByType(&data,PlSharedFontType_ChineseSimplified))) return false;
    font=TTF_OpenFontRW(SDL_RWFromConstMem(data.address,data.size),1,27);
    fontSmall=TTF_OpenFontRW(SDL_RWFromConstMem(data.address,data.size),1,21);
    for (std::size_t i=0; i<Config::DOWNLOAD_COUNT; ++i) {
        covers[i]=IMG_LoadTexture(renderer,Config::DOWNLOADS[i].coverPath);
    }
    backgroundTexture=IMG_LoadTexture(renderer,"romfs:/background.png");
    if (Mix_OpenAudio(48000,MIX_DEFAULT_FORMAT,2,4096)==0) {
        backgroundMusic=Mix_LoadMUS("sdmc:/switch/3DS_Eshop_XPG/bgm.mp3");
        if(backgroundMusic) Mix_PlayMusic(backgroundMusic,-1);
    }
    return font && fontSmall;
}

void shutdownUi() {
    Mix_HaltMusic();
    if(backgroundMusic) { Mix_FreeMusic(backgroundMusic); backgroundMusic=nullptr; }
    Mix_CloseAudio();
    // Clear and present once before tearing down the EGL-backed renderer.
    if (renderer) {
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
    for (auto*& cover : covers) {
        if (cover) SDL_DestroyTexture(cover);
        cover=nullptr;
    }
    if(backgroundTexture) { SDL_DestroyTexture(backgroundTexture); backgroundTexture=nullptr; }
    if(fontSmall) TTF_CloseFont(fontSmall);
    if(font) TTF_CloseFont(font);
    if(renderer) SDL_DestroyRenderer(renderer);
    if(window) SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    plExit();
    SDL_Quit();
}

int chooseDownload() {
    constexpr std::size_t ITEMS_PER_PAGE=9;
    std::size_t selected=0;
    while(appletMainLoop()) {
        begin("Download Library");
        const std::size_t page=selected/ITEMS_PER_PAGE;
        const std::size_t pageCount=(Config::DOWNLOAD_COUNT+ITEMS_PER_PAGE-1)/ITEMS_PER_PAGE;
        const std::size_t first=page*ITEMS_PER_PAGE;
        const std::size_t possibleEnd=first+ITEMS_PER_PAGE;
        const std::size_t end=possibleEnd<Config::DOWNLOAD_COUNT?possibleEnd:Config::DOWNLOAD_COUNT;
        for(std::size_t i=first;i<end;++i) {
            const std::size_t local=i-first;
            const int col=local%3, row=local/3;
            const int x=27+col*414, y=88+row*181;
            SDL_SetRenderDrawColor(renderer,14,17,25,255); SDL_Rect card{x,y,396,162}; SDL_RenderFillRect(renderer,&card);
            frame(x,y,396,162,i==selected?BLUE:SDL_Color{75,82,98,255},i==selected?4:2);
            icon(x+15,y+15,120,132,i);
            SDL_Rect textClip{x+145,y+8,240,145};
            SDL_RenderSetClipRect(renderer,&textClip);
            text(Config::DOWNLOADS[i].title,x+150,y+28,WHITE,fontSmall);
            text(Config::DOWNLOADS[i].description,x+150,y+72,GREY,fontSmall);
            text("Ready to download",x+150,y+112,GREEN,fontSmall);
            SDL_RenderSetClipRect(renderer,nullptr);
        }
        const std::string counter="Page "+std::to_string(page+1)+"/"+std::to_string(pageCount)+
                                  "   "+std::to_string(selected+1)+"/"+std::to_string(Config::DOWNLOAD_COUNT);
        text(counter,1035,24,GREY,fontSmall);
        footer("D-Pad  Select      A  Details      Y  Check update      +  Exit");
        SDL_RenderPresent(renderer);
        padUpdate(&pad); const u64 k=padGetButtonsDown(&pad);
        if(k&HidNpadButton_Plus) return -1;
        if(k&HidNpadButton_Y) return -2;
        if(k&HidNpadButton_A) return (int)selected;
        if(k&(HidNpadButton_Left|HidNpadButton_StickLLeft)) selected=selected?selected-1:Config::DOWNLOAD_COUNT-1;
        if(k&(HidNpadButton_Right|HidNpadButton_StickLRight)) selected=(selected+1)%Config::DOWNLOAD_COUNT;
        if(k&(HidNpadButton_Up|HidNpadButton_StickLUp)) selected=selected>=3?selected-3:selected;
        if(k&(HidNpadButton_Down|HidNpadButton_StickLDown)) if(selected+3<Config::DOWNLOAD_COUNT) selected+=3;
    }
    return -1;
}

int details(const Config::DownloadItem& item, std::size_t index) {
    int mirror=0;
    const bool hasMirror=item.mirrorUrl && item.mirrorUrl[0];
    while(appletMainLoop()) {
        begin(item.title);
        frame(28,88,800,540,{75,82,98,255},2); icon(48,108,760,500,index);
        frame(855,88,397,540,{75,82,98,255},2);
        text(item.title,880,120,WHITE);
        text(item.description,880,170,GREY,fontSmall);
        if(hasMirror) {
            for(int line=0;line<2;++line) {
                const int x=880+line*174;
                SDL_SetRenderDrawColor(renderer,line==0?35:105,line==0?95:45,line==0?170:145,255);
                SDL_Rect tile{x,225,160,105}; SDL_RenderFillRect(renderer,&tile);
                frame(x,225,160,105,line==mirror?BLUE:SDL_Color{95,102,118,255},line==mirror?4:2);
                filledCircleRGBA(renderer,x+80,260,18,240,240,245,255);
                text("Line "+std::to_string(line+1),x+46,283,WHITE,fontSmall);
            }
        } else {
            SDL_SetRenderDrawColor(renderer,20,48,145,255);
            SDL_Rect button{880,235,345,70}; SDL_RenderFillRect(renderer,&button);
            frame(880,235,345,70,BLUE,2);
            text("Download / Install",935,252,WHITE);
        }
        text("Destination",880,350,GREY,fontSmall);
        const std::string directory=(item.destinationPath&&item.destinationPath[0])?item.destinationPath:Config::DEST_DIR;
        const std::string destination=item.extractZip?directory:(directory+item.fileName);
        text(destination,880,390,WHITE,fontSmall);
        footer(hasMirror?"Left/Right  Select icon      A  Download      B  Back":"A  Download      B  Back");
        SDL_RenderPresent(renderer);
        padUpdate(&pad); const u64 k=padGetButtonsDown(&pad);
        if(hasMirror && (k&(HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_StickLLeft|HidNpadButton_StickLRight))) mirror=1-mirror;
        if(k&HidNpadButton_A) return mirror;
        if(k&HidNpadButton_B) return -1;
    }
    return -1;
}

bool promptUpdate(const UpdateInfo& update) {
    while(appletMainLoop()) {
        begin("Software Update");
        frame(215,150,850,390,{75,82,98,255},2);
        text("A new version is available",380,205,WHITE);
        text("Installed: 1.2.5",420,280,GREY,fontSmall);
        text("Latest: "+update.version,420,320,GREEN,fontSmall);
        SDL_SetRenderDrawColor(renderer,20,48,145,255);
        SDL_Rect b{385,390,510,70}; SDL_RenderFillRect(renderer,&b);
        frame(385,390,510,70,BLUE,2); text("Download update",500,407,WHITE);
        footer("A  Update now      B  Skip"); SDL_RenderPresent(renderer);
        padUpdate(&pad); const u64 k=padGetButtonsDown(&pad);
        if(k&HidNpadButton_A) return true;
        if(k&HidNpadButton_B) return false;
    }
    return false;
}
}

void uiRenderDownloadProgress(int percent,double downloadedMb,double totalMb,double speedMb) {
    begin("Downloading");
    text("Downloading package...",90,150,WHITE);
    SDL_SetRenderDrawColor(renderer,30,35,48,255); SDL_Rect bg{90,245,1100,42}; SDL_RenderFillRect(renderer,&bg);
    SDL_SetRenderDrawColor(renderer,55,95,255,255); SDL_Rect fill{90,245,11*percent,42}; SDL_RenderFillRect(renderer,&fill);
    frame(90,245,1100,42,{110,120,145,255},2);
    text(std::to_string(percent)+"%",600,310,WHITE);
    char info[128]; snprintf(info,sizeof(info),"%.1f MB / %.1f MB       %.1f MB/s",downloadedMb,totalMb,speedMb);
    text(info,420,365,GREY,fontSmall); footer("B  Cancel download"); SDL_RenderPresent(renderer);
}

int main(int argc,char** argv) {
    padConfigureInput(1,HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
    const bool romfsReady=R_SUCCEEDED(romfsInit());
    if(!initUi()) return 1;
    const bool socketReady=R_SUCCEEDED(socketInitializeDefault());
    const bool nifmReady=R_SUCCEEDED(nifmInitialize(NifmServiceType_User));
    const bool curlReady=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
    mkdir("sdmc:/switch",0777); mkdir(Config::TEMP_DIR,0777);
    mkdir("sdmc:/switch/3DS_Eshop_XPG/images",0777);
    mkdir("sdmc:/roms",0777); mkdir(Config::DEST_DIR,0777);
    mkdir("sdmc:/gbastation",0777);
    mkdir("sdmc:/gbastation/3ds",0777);
    bool exitRequested=false;
    while(appletMainLoop() && !exitRequested) {
        const int choice=chooseDownload();
        if(choice==-1) { exitRequested=true; break; }
        if(choice==-2) {
            const UpdateInfo update=checkForUpdate();
            if(update.available && promptUpdate(update)) {
                std::string error;
                const std::string currentPath=(argc>0 && argv && argv[0])?argv[0]:"";
                const bool updated=installUpdate(update,currentPath,error);
                showMessage(updated?"Update ready":"Update failed",
                            updated?"Please Restart Software":error,
                            updated?GREEN:RED);
                if(updated) { exitRequested=true; break; }
            } else {
                showMessage("Software Update",update.error.empty()?"No new version is available":update.error,
                            update.error.empty()?GREEN:RED);
            }
        } else if(choice>=0) {
            const int mirror=details(Config::DOWNLOADS[choice],choice);
            if(mirror<0) continue;
            const auto& item=Config::DOWNLOADS[choice];
            const bool archive=item.extractZip;
            const std::string destination=(item.destinationPath&&item.destinationPath[0])?item.destinationPath:Config::DEST_DIR;
            ensureDirectory(destination);
            const std::string output=archive?(std::string(Config::TEMP_DIR)+"/"+item.fileName)
                                            :(destination+item.fileName);
            std::string error;
            const char* selectedUrl=mirror==1?item.mirrorUrl:item.url;
            bool ok=downloadFile(selectedUrl,output,error);
            if(ok&&archive) {
                begin("Installing cheats");
                text("Extracting ZIP...",500,320,WHITE);
                SDL_RenderPresent(renderer);
                ok=extractZipTo(output,destination,error);
                remove(output.c_str());
            }
            showMessage(ok?"Download complete":"Download failed",
                        ok?(archive?("Files extracted to "+destination):("File saved to "+destination)):error,
                        ok?GREEN:RED);
        }

        while(appletMainLoop()) {
            padUpdate(&pad); const u64 keys=padGetButtonsDown(&pad);
            if(keys&HidNpadButton_B) break;
            if(keys&HidNpadButton_Plus) { exitRequested=true; break; }
        }
    }
    if(curlReady) curl_global_cleanup();
    if(nifmReady) nifmExit();
    if(socketReady) socketExit();
    shutdownUi();
    if(romfsReady) romfsExit();
    return 0;
}
