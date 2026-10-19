# Bundled ExifTool

`rcaiman-projects` uses a bundled ExifTool executable for reproducible
metadata extraction. Official binary releases must include a known ExifTool
version. Source checkouts do not need to commit ExifTool binaries.

## Expected Local Layout

```text
tools/exiftool/
  README.md
  windows/
    exiftool.exe
    exiftool_files/
  unix/
    exiftool
    lib/
```

## Windows

1. Download the current Windows 64-bit package from <https://exiftool.org/>.
2. Extract it into `tools/exiftool/windows/`.
3. Rename `exiftool(-k).exe` to `exiftool.exe`.
4. Keep `exiftool_files/` next to `exiftool.exe`.

## Linux and macOS

1. Download the full platform-independent ExifTool distribution from
   <https://exiftool.org/>.
2. Copy `exiftool` and `lib/` into `tools/exiftool/unix/`.
3. Ensure the script is executable:

```sh
chmod +x tools/exiftool/unix/exiftool
```

The current build expects the bundled script to use the system Perl on
Linux/macOS.
