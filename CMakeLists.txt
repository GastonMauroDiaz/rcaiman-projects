cmake_minimum_required(VERSION 3.19)
project(rcaiman_projects LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

# Locate a library installed via MSYS2 / MinGW64 on Windows.
macro(find_msys2_library TARGET_NAME HEADER_FILE LIBRARY_FILE)
    if(WIN32)
        find_path(${TARGET_NAME}_INCLUDE_DIR ${HEADER_FILE}
            PATHS "C:/msys64/mingw64/include"
            NO_DEFAULT_PATH
        )
        find_library(${TARGET_NAME}_LIBRARY ${LIBRARY_FILE}
            PATHS "C:/msys64/mingw64/lib"
            NO_DEFAULT_PATH
        )
        if(NOT ${TARGET_NAME}_INCLUDE_DIR OR NOT ${TARGET_NAME}_LIBRARY)
            find_path(${TARGET_NAME}_INCLUDE_DIR ${HEADER_FILE})
            find_library(${TARGET_NAME}_LIBRARY ${LIBRARY_FILE})
        endif()
        if(NOT ${TARGET_NAME}_INCLUDE_DIR OR NOT ${TARGET_NAME}_LIBRARY)
            message(FATAL_ERROR "Cannot find ${TARGET_NAME}.\n"
                "Install via MSYS2: pacman -S mingw-w64-x86_64-${LIBRARY_FILE}")
        endif()
        add_library(${TARGET_NAME} UNKNOWN IMPORTED)
        set_target_properties(${TARGET_NAME} PROPERTIES
            IMPORTED_LOCATION "${${TARGET_NAME}_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${${TARGET_NAME}_INCLUDE_DIR}"
        )
    else()
        find_package(${TARGET_NAME} REQUIRED)
    endif()
endmacro()

# TIFF: #include <tiffio.h> → tiffio.h is directly in mingw64/include/
find_msys2_library(TIFF tiffio.h tiff)
# LibRaw: #include <libraw/libraw.h> → search for libraw/libraw.h in mingw64/include/
find_msys2_library(LibRaw libraw/libraw.h raw_r)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(rcaiman_projects
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    imageviewer.h imageviewer.cpp
    pointcollection.h pointcollection.cpp
    project.h project.cpp
    rasterdata.h rasterdata.cpp
    rasterrenderer.h rasterrenderer.cpp
    tiffreader.h tiffreader.cpp
    rawreader.h rawreader.cpp
    collectiondialog.h collectiondialog.cpp
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
        TIFF
        LibRaw
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