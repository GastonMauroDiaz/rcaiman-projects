# Third-party software and licenses

rcaiman-projects is built with the work of the following projects and their
contributors. We gratefully acknowledge their contributions.

This overview covers the application's direct dependencies and supporting
libraries used in the Windows distribution. Components and versions may vary
with the build and platform. The linked upstream license texts are included
with the application; their terms and copyright notices remain unchanged.

## Qt

**The Qt Company and contributors** - application framework, user interface,
graphics, and platform integration.

- [Project website](https://www.qt.io/)
- [Qt license information and LGPL/GPL texts](licenses/qt/LICENSE.txt)
- [Third-party code used in Qt](https://doc.qt.io/qt-6/licenses-used-in-qt.html)

## LibRaw

**LibRaw LLC and contributors** - RAW sensor data extraction and RAW metadata.

- [Project website](https://www.libraw.org/)
- [GNU LGPL version 2.1](licenses/libraw/LGPL-2.1.txt)
- [Alternative CDDL version 1.0](licenses/libraw/CDDL-1.0.txt)

## libtiff

**The libtiff authors and contributors** - TIFF image reading and writing.

- [Project website](https://libtiff.gitlab.io/libtiff/)
- [Copyright notices and license](licenses/libtiff/LICENSE.txt)

## yaml-cpp

**Jesse Beder and contributors** - YAML reading and writing.

- [Project website](https://github.com/jbeder/yaml-cpp)
- [MIT license](licenses/yaml-cpp/LICENSE.txt)

## ExifTool

**Phil Harvey** - metadata extraction. ExifTool is free software distributed
under the same terms as Perl.

- [Project website](https://exiftool.org/)
- [Perl Artistic License](licenses/exiftool/Artistic.txt)
- [Perl GNU GPL license option](licenses/exiftool/Copying.txt)
- [Windows package credits and launcher notice](licenses/exiftool/windows.txt)

The Windows package also includes Strawberry Perl and a launcher by Oliver
Betz. The upstream `Licenses_Strawberry_Perl.zip` archive remains alongside
ExifTool in the distribution and contains additional component notices.

## Supporting libraries

These projects provide image codecs, compression, color management, and runtime
support. The following license files accompany the Windows library packages;
some contain notices for multiple parts of their upstream projects.

- **libdeflate**: [license](licenses/runtime/libdeflate/COPYING) - [website](https://github.com/ebiggers/libdeflate).
- **JBIG-KIT**: [license](licenses/runtime/jbigkit/COPYING) - [website](https://www.cl.cam.ac.uk/~mgk25/jbigkit/).
- **libjpeg-turbo**: [license overview](licenses/runtime/libjpeg-turbo/LICENSE.md), [IJG notices](licenses/runtime/libjpeg-turbo/README.ijg), [project README](licenses/runtime/libjpeg-turbo/README.md), [SIMD notices](licenses/runtime/libjpeg-turbo/simd/jsimdext.inc) - [website](https://libjpeg-turbo.org/).
- **Lerc**: [license and notices](licenses/runtime/lerc/LICENSE) - [website](https://github.com/Esri/lerc).
- **XZ Utils / liblzma**: [license overview](licenses/runtime/xz/COPYING), [0BSD](licenses/runtime/xz/COPYING.0BSD), [LGPL v2.1](licenses/runtime/xz/COPYING.LGPLv2.1), [GPL v2](licenses/runtime/xz/COPYING.GPLv2), [GPL v3](licenses/runtime/xz/COPYING.GPLv3) - [website](https://tukaani.org/xz/).
- **libwebp**: [license](licenses/runtime/libwebp/COPYING) - [website](https://developers.google.com/speed/webp).
- **zlib**: [license](licenses/runtime/zlib/LICENSE) - [website](https://zlib.net/).
- **Zstandard**: [license](licenses/runtime/zstd/LICENSE) - [website](https://facebook.github.io/zstd/).
- **Little CMS**: [license](licenses/runtime/lcms2/LICENSE), [additional fast-float license](licenses/runtime/lcms2/LICENSE-fast_float) - [website](https://www.littlecms.com/).
- **libpng**: [license](licenses/runtime/libpng/LICENSE) - [website](https://www.libpng.org/pub/png/libpng.html).
- **GCC runtime libraries**, including OpenMP support: [package notice](licenses/runtime/gcc-libs/README), [GPL v3](licenses/runtime/gcc-libs/COPYING3), [GCC Runtime Library Exception](licenses/runtime/gcc-libs/COPYING.RUNTIME), [LGPL v2.1](licenses/runtime/gcc-libs/COPYING.LIB) - [website](https://gcc.gnu.org/).
- **MinGW-w64 winpthreads**: [license and notices](licenses/runtime/libwinpthread/COPYING) - [website](https://www.mingw-w64.org/).
