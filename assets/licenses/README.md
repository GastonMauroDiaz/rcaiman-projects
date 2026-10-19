# Maintaining third-party notices

The About dialog opens `THIRD_PARTY_NOTICES.md` from Qt resources. Its relative
`licenses/` links refer to this directory. The same document and license files
are installed together so the notices can also be read outside the application.

Keep upstream license files verbatim. Do not reword their terms or copyright
notices. When a dependency changes, review its license files and update the
corresponding resources and index. Do not fetch licenses at application startup.

## Provenance of the initial copies

- `qt/LICENSE.txt`: `C:/Qt/Licenses/LICENSE`, supplied with the Qt 6.11.1
  installation used for this build. It includes Qt's licensing overview and the
  LGPL v3 and GPL v3 texts.
- `libraw/`: `mingw64/share/licenses/libraw/` from MSYS2.
- `libtiff/LICENSE.txt`: MSYS2 `mingw64/share/licenses/libtiff/LICENSE.md`.
- `yaml-cpp/LICENSE.txt`: the vendored `third_party/yaml-cpp-0.8.0/LICENSE`.
- `exiftool/Artistic.txt` and `exiftool/Copying.txt`: the `perl/Artistic` and
  `perl/Copying` entries in the ExifTool 13.59 Windows bundle's
  `Licenses_Strawberry_Perl.zip` archive.
- `exiftool/windows.txt`: that bundle's `readme_windows.txt`.
- `runtime/`: license directories from MSYS2 `mingw64/share/licenses/` for
  the listed image, compression, color, and runtime library packages.

## Release review

This index is not an automated license-compliance audit. Review the actual
release contents, including Qt's third-party code and any additional plugins,
runtime libraries, assets, and ExifTool dependencies. Qt supplies per-module
SBOM documents and third-party attribution documentation; ExifTool's Windows
bundle supplies the Strawberry Perl license archive. Preserve those upstream
notices in the release as appropriate for the components distributed.

The test suite checks that all local links in the index resolve to bundled
resources, but cannot determine which components a future release will include.
