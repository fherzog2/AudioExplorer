# AudioExplorer

AudioExplorer is a desktop application for quickly finding audio files based on their meta data, like searching for a certain artist or genre.

## Dependencies

- [Qt](https://www.qt.io/)
- [TagLib](https://taglib.org/)

## Build instructions

### Windows

On windows, a statically linked executable is preferred. It can be build using the following commands. The dependencies are retrieved through vcpkg.

```console
vcpkg install qt5-base qt5-svg taglib --triplet x64-windows-static

cmake %AudioExplorer_PATH% -G "Visual Studio 14 2015 Win64" -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC_RUNTIME=ON
```

### Linux

```console
cmake $AudioExplorer_PATH -G "Unix Makefiles"
```
