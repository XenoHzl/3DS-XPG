# Windows 编译步骤

## 1. 安装 devkitPro

使用 devkitPro 官方提供的 Windows 环境，然后打开 **devkitPro MSYS2** shell。

libnx 官方仓库的安装说明仍指向 Switchbrew/devkitPro 环境。

## 2. 更新并安装 Switch 工具

在 devkitPro MSYS2 内运行：

    pacman -Syu
    pacman -S --needed switch-dev switch-curl switch-zlib switch-minizip

如果第一次 `pacman -Syu` 要求关闭 shell，请关闭后重新打开 devkitPro MSYS2，再继续。

## 3. 检查环境

进入项目目录：

    cd /你的路径/XPerfect_3DS_Downloader_V1_1

执行：

    bash check_build_env.sh

全部显示 `[OK]` 后继续。

## 4. 编译

    make clean
    make -j4

成功后项目目录应该出现：

    XPerfect3DSDownloader.nro

## 5. 放入 SD 卡

    sdmc:/switch/XPerfect3DSDownloader/XPerfect3DSDownloader.nro

## 6. 测试下载

先修改：

    include/config.hpp

例如电脑 IP 是 `192.168.1.55`：

    constexpr const char* ZIP_URL =
        "http://192.168.1.55:8000/3ds.zip";

然后把 `3ds.zip` 放进：

    server/

运行：

    server/Windows_Start_Server.bat

Switch 与电脑必须能互相访问这个局域网地址。

程序执行后会：

    下载 3ds.zip
        ↓
    sdmc:/switch/XPerfect3DSDownloader/download.zip
        ↓
    解压内容
        ↓
    sdmc:/roms/3ds/
        ↓
    删除临时 download.zip

下载过程中现在可以按 **B** 取消。

> 请只复制或分发你有权使用的 ROM/镜像/文件。
