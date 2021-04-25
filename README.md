# AudioExplorer

AudioExplorer is a desktop application for quickly finding audio files based on their meta data, like searching for a certain artist or genre.

## Dependencies

- [Qt](https://www.qt.io/)
- [TagLib](https://taglib.org/)

## Build instructions

### Windows

On windows, a statically linked executable is preferred. It can be build using the following commands. The dependencies are retrieved through vcpkg.

```console
vcpkg install qt5-base qt5-svg qt5-tools qt5-translations taglib --triplet x64-windows-static

cmake %AudioExplorer_PATH% -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC_RUNTIME=ON
```

### Linux

```console
apt install qtbase5-dev libqt5svg5-dev libtag1-dev

cmake $AudioExplorer_PATH -G "Unix Makefiles"
```
