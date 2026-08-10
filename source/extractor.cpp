#include "extractor.hpp"
#include <minizip/unzip.h>
#include <sys/stat.h>
#include <cstdio>

static bool unsafe(const std::string& p) {
    if(p.empty() || p[0]=='/' || p[0]=='\\') return true;
    size_t i=0;
    while(i<p.size()){
        size_t e=p.find_first_of("/\\",i);
        std::string part=p.substr(i,e==std::string::npos?std::string::npos:e-i);
        if(part=="..") return true;
        if(e==std::string::npos) break;
        i=e+1;
    }
    return false;
}

static void mkdirs(const std::string& p) {
    std::string cur;
    for(char c:p){cur+=c;if(c=='/')mkdir(cur.c_str(),0777);}
    mkdir(cur.c_str(),0777);
}

bool extractZipTo(const std::string& zipPath,const std::string& dest,std::string& error) {
    unzFile z=unzOpen(zipPath.c_str());
    if(!z){error="cannot open ZIP";return false;}
    int rc=unzGoToFirstFile(z);
    char name[4096];

    while(rc==UNZ_OK){
        unz_file_info info{};
        if(unzGetCurrentFileInfo(z,&info,name,sizeof(name),nullptr,0,nullptr,0)!=UNZ_OK){
            error="cannot read ZIP entry";unzClose(z);return false;
        }
        std::string entry(name);
        if(unsafe(entry)){error="unsafe ZIP path: "+entry;unzClose(z);return false;}
        std::string target=dest+entry;
        bool dir=!entry.empty()&&(entry.back()=='/'||entry.back()=='\\');
        if(dir) mkdirs(target);
        else {
            size_t slash=target.find_last_of('/');
            if(slash!=std::string::npos) mkdirs(target.substr(0,slash));
            if(unzOpenCurrentFile(z)!=UNZ_OK){error="cannot open ZIP file";unzClose(z);return false;}
            FILE* f=fopen(target.c_str(),"wb");
            if(!f){unzCloseCurrentFile(z);error="cannot write: "+target;unzClose(z);return false;}
            char buf[65536]; int n;
            while((n=unzReadCurrentFile(z,buf,sizeof(buf)))>0){
                if(fwrite(buf,1,n,f)!=(size_t)n){fclose(f);unzCloseCurrentFile(z);unzClose(z);error="SD write failed";return false;}
            }
            fclose(f); unzCloseCurrentFile(z);
            if(n<0){unzClose(z);error="ZIP read error";return false;}
        }
        rc=unzGoToNextFile(z);
    }
    unzClose(z);
    if(rc!=UNZ_END_OF_LIST_OF_FILE){error="ZIP iteration error";return false;}
    return true;
}
