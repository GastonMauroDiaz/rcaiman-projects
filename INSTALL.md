# rcaiman_projects — Dependencies

## MSYS2 / MinGW (64-bit)

The project requires Qt6, libtiff and LibRaw installed via MSYS2.

```bash
pacman -S mingw-w64-x86_64-qt6-base
pacman -S mingw-w64-x86_64-libtiff
pacman -S mingw-w64-x86_64-libraw
```

## Build

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Windows Portable Release

Build the Release configuration with the Qt Creator MinGW kit, then run this
from 64-bit PowerShell in the repository root:

```powershell
.\tools\package-windows.ps1
```

The script reads the tool locations from the existing CMake cache, builds and
runs CTest, installs the application with Qt's deployment support, and creates
a portable ZIP plus its SHA-256 checksum in `build/releases/`. It does not
publish to GitHub, create tags, change global settings, or overwrite an existing
release archive. `-BuildDirectory`, `-OutputDirectory`, and `-Version` can be
specified explicitly. The version must match About and the packaged README.
The build executable must not be running when rebuilding it. To keep using the
application during packaging, configure a separate Release build directory with
the same Qt/MinGW kit and pass it with `-BuildDirectory`.

The package includes the Qt runtime and plugins, the RAW/TIFF dependencies,
ExifTool and its companion files, the example project, and license texts.
The project README template is embedded in the executable.

Before archiving, the script checks the import dependencies of all packaged
executables and DLLs. It also runs metadata/TIFF/project, license, and measurement
tests with development directories removed from PATH, and starts the application
headlessly. Temporary test executables are not included in the ZIP. This is not
a substitute for testing the extracted ZIP on a separate Windows machine.

`BUILD-INFO.json` records the source commit and whether the working tree had
uncommitted changes. Commit release fixes before making the final package and
tagging the matching source revision. Do not upload the intermediate staging
directories; upload only the final ZIP and checksum. See `assets/licenses/README.md`
for the separate review of third-party notices needed for a release.
