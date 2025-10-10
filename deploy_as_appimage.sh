#! /bin/bash

set -x
set -e

# download and run linuxdeploy

wget -nc -nv https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -nc -nv https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

chmod +x linuxdeploy*.AppImage

export QMAKE=qmake6

./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage
