# AudioExplorer

AudioExplorer is a desktop application for quickly finding audio files based on their meta data, like searching for a certain artist or genre.

## Dependencies

- [Qt](https://www.qt.io/)
- [TagLib](https://taglib.org/)

## Build instructions

### Windows

On Windows, the dependency paths may need to be specified if CMake doesn't find them automatically.

```console
cmake %AudioExplorer_PATH% -G "Visual Studio 14 2015 Win64" -DQt5Widgets_DIR=%QT_DIR%\lib\cmake\Qt5Widgets -DQt5Svg_DIR=%QT_DIR%\lib\cmake\Qt5Svg -DTAGLIB_DIR=%TAGLIB_INSTALL_DIR%
```

### Linux

```console
cmake $AudioExplorer_PATH -G "Unix Makefiles"
```
