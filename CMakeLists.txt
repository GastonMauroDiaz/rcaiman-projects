cmake_minimum_required(VERSION 3.19)
project(rcaiman_projects LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

# Locate libtiff (MSYS2 / MinGW64 on Windows, system-wide on Linux/macOS).
if(WIN32)
    # MSYS2 installs to C:/msys64/mingw64
    find_path(TIFF_INCLUDE_DIR tiff.h
        PATHS "C:/msys64/mingw64/include"
        NO_DEFAULT_PATH
    )
    find_library(TIFF_LIBRARY tiff
        PATHS "C:/msys64/mingw64/lib"
        NO_DEFAULT_PATH
    )
    if(NOT TIFF_INCLUDE_DIR OR NOT TIFF_LIBRARY)
        # Fallback: try system paths
        find_path(TIFF_INCLUDE_DIR tiff.h)
        find_library(TIFF_LIBRARY tiff)
    endif()
else()
    find_package(TIFF REQUIRED)
endif()

if(WIN32)
    if(NOT TIFF_INCLUDE_DIR OR NOT TIFF_LIBRARY)
        message(FATAL_ERROR "Cannot find libtiff. Install it via MSYS2:\n"
            "  pacman -S mingw-w64-x86_64-libtiff\n"
            "Then reconfigure.")
    endif()
    # Create an imported target matching TIFF::TIFF for consistency
    add_library(TIFF::TIFF UNKNOWN IMPORTED)
    set_target_properties(TIFF::TIFF PROPERTIES
        IMPORTED_LOCATION "${TIFF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIR}"
    )
endif()

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(rcaiman_projects
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    imageviewer.h imageviewer.cpp
    project.h project.cpp
    rasterdata.h rasterdata.cpp
    rasterrenderer.h rasterrenderer.cpp
    tiffreader.h tiffreader.cpp
    resources.qrc
)

target_include_directories(rcaiman_projects
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(rcaiman_projects PRIVATE
    TEMPLATES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates"
)

qt_add_translations(
    TARGETS rcaiman_projects
    TS_FILES rcaiman_projects_es_AR.ts
)

target_link_libraries(rcaiman_projects
    PRIVATE
        Qt::Core
        Qt::Widgets
        TIFF::TIFF
)

install(TARGETS rcaiman_projects
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET rcaiman_projects
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})