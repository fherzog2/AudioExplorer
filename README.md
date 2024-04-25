# AudioExplorer

AudioExplorer is a desktop application for quickly finding audio files based on their meta data, like searching for a certain artist or genre.

## Dependencies

- [Qt](https://www.qt.io/)
- [TagLib](https://taglib.org/)

## Build instructions

### Windows

On Windows, vcpkg can be used to compile the dependencies.

```console
vcpkg install qtbase qtsvg qttools qttranslations taglib gtest --triplet x64-windows

cmake %AudioExplorer_PATH% -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
```

### Linux

```console
apt install qt6-base-dev qt6-svg-dev qt6-tools-dev libtag1-dev libgtest-dev

cmake $AudioExplorer_PATH -G "Unix Makefiles"
```
