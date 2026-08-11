# 3DS Eshop XPG

A graphical download manager for Nintendo Switch homebrew, built with libnx and SDL2.

Features include a paginated library, D-pad navigation, cover images, background music,
mirrors, ZIP extraction, safe file replacement, and GitHub Release self-updates.

Version 1.2.1 adds a per-item destination directory and an explicit ZIP extraction flag.

## Configure downloads

Edit `include/config.hpp`. Add only homebrew, patches, or other files you own or are
authorized to redistribute. The repository intentionally ships with a harmless example URL.

Runtime assets can be placed in:

    sdmc:/switch/3DS_Eshop_XPG/images/
    sdmc:/switch/3DS_Eshop_XPG/bgm.mp3

## Build

Open a devkitPro Switch development shell and run:

    make clean
    make -j4

The generated file is `3DS_Eshop_XPG.nro`.

See `BUILD_WINDOWS.md` for the Windows/devkitPro workflow.

## Updates

The application checks releases from `XenoHzl/3DS-XPG`. A release must contain an asset
named exactly `3DS_Eshop_XPG.nro`. Use a version tag such as `v1.2.0`.

## Legal

This project does not include copyrighted games or download links to them. Use it only for
content you are legally entitled to copy and distribute.
