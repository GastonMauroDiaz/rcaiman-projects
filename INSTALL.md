# rcaiman_projects — Dependencies

## MSYS2 / MinGW (64-bit)

The project requires Qt6 and libtiff installed via MSYS2.

```bash
pacman -S mingw-w64-x86_64-qt6-base
pacman -S mingw-w64-x86_64-libtiff
```

## Build

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Future dependencies (planned)

```bash
pacman -S mingw-w64-x86_64-libraw
```
</write_to_file>