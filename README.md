# AudioExplorer

AudioExplorer is a desktop application for quickly finding audio files based on their meta data, like searching for a certain artist or genre.

## Dependencies

- [Qt](https://www.qt.io/)
- [TagLib](https://taglib.org/)

## Build instructions

### Windows

On Windows, vcpkg can be used to compile the dependencies.

```console
vcpkg install qt5-base qt5-svg qt5-tools qt5-translations taglib --triplet x64-windows

cmake %AudioExplorer_PATH% -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
```

### Linux

```console
apt install qtbase5-dev libqt5svg5-dev libtag1-dev

cmake $AudioExplorer_PATH -G "Unix Makefiles"
```
