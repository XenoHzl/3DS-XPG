#include <switch.h>
extern "C" {
#include <switch/runtime/env.h>
}
#include <cstdio>
#include <string>

static bool copyFile(const std::string& source, const std::string& destination) {
    FILE* input=fopen(source.c_str(),"rb"); if(!input) return false;
    FILE* output=fopen(destination.c_str(),"wb"); if(!output){fclose(input);return false;}
    char buffer[64*1024]; bool ok=true;
    while(true){size_t n=fread(buffer,1,sizeof(buffer),input);if(n&&fwrite(buffer,1,n,output)!=n){ok=false;break;}if(n<sizeof(buffer)){if(ferror(input))ok=false;break;}}
    if(fflush(output)!=0) ok=false; fclose(output); fclose(input); return ok;
}

int main() {
    const char* targetFile="sdmc:/switch/3DS_Eshop_XPG/update_target.txt";
    FILE* file=fopen(targetFile,"rb"); if(!file) return 1;
    char path[1024]{}; const size_t n=fread(path,1,sizeof(path)-1,file); fclose(file);
    if(!n) return 1;
    const std::string target(path,n), pending=target+".new", backup=target+".bak";
    svcSleepThread(500000000ULL);
    remove(backup.c_str());
    if(!copyFile(target,backup)) return 2;
    if(!copyFile(pending,target)){copyFile(backup,target);return 3;}
    remove(pending.c_str()); remove(targetFile);
    envSetNextLoad(target.c_str(),target.c_str());
    return 0;
}
