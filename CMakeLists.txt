cmake_minimum_required(VERSION 3.19)
project(rcaiman_projects LANGUAGES CXX)
include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp-0.8.0)

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
    apppreferences.h apppreferences.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    imageviewer.h imageviewer.cpp
    imagelayer.h imagelayer.cpp
    layer.h layer.cpp
    layerstyle.h
    layerpropertieswidget.h layerpropertieswidget.cpp
    layermanager.h layermanager.cpp
    layersdock.h layersdock.cpp
    imagelayerpropertieswidget.h imagelayerpropertieswidget.cpp
    pointcollection.h pointcollection.cpp
    pointcsvcreationdialog.h pointcsvcreationdialog.cpp
    pointitem.h pointitem.cpp
    pointlayer.h pointlayer.cpp
    pointlayerpropertieswidget.h pointlayerpropertieswidget.cpp
    pointattributetablemodel.h pointattributetablemodel.cpp
    pointattributetablewindow.h pointattributetablewindow.cpp
    histogramdock.h histogramdock.cpp
    previewdock.h previewdock.cpp
    project.h project.cpp
    projectfilesystemmodel.h projectfilesystemmodel.cpp
    projectcleandialog.h projectcleandialog.cpp
    projectcleanupdialog.h projectcleanupdialog.cpp
    newprojectdialog.h newprojectdialog.cpp
    rasterdata.h rasterdata.cpp
    rasterrenderer.h rasterrenderer.cpp
    tiffreader.h tiffreader.cpp
    tiffwriter.h tiffwriter.cpp
    pgmreader.h pgmreader.cpp
    rawreader.h rawreader.cpp
    collectiondialog.h collectiondialog.cpp
    collectioneditdialog.h collectioneditdialog.cpp
    collectiongraphdialog.h collectiongraphdialog.cpp
    collectionautoload.h collectionautoload.cpp
    collectionstylefile.h collectionstylefile.cpp
    collectionstyledialog.h collectionstyledialog.cpp
    metadataregistry.h metadataregistry.cpp
    metadatarulesdialog.h metadatarulesdialog.cpp
    registryvalidationdialog.h registryvalidationdialog.cpp
    collectionvalidation.h collectionvalidation.cpp
    collectionvalidationdialog.h collectionvalidationdialog.cpp
    acquisitionmetadata.h acquisitionmetadata.cpp
    legacyjpegimportdialog.h legacyjpegimportdialog.cpp
    legacyjpegimporter.h legacyjpegimporter.cpp
    hspimportdialog.h hspimportdialog.cpp
    hspprojectimporter.h hspprojectimporter.cpp
    exampleprojectsdialog.h exampleprojectsdialog.cpp
    shortcuttext.h shortcuttext.cpp
    resources.qrc
)

if(WIN32)
    target_sources(rcaiman_projects PRIVATE
        assets/branding/rcaiman-projects.rc
    )
endif()

set_target_properties(rcaiman_projects PROPERTIES
    OUTPUT_NAME "rcaiman-projects"
)

target_include_directories(rcaiman_projects
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(rcaiman_projects PRIVATE
    TEMPLATES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates"
    EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/examples"
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
        yaml-cpp
)

if(WIN32)
    set(EXIFTOOL_BUNDLE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/exiftool/windows")
elseif(APPLE)
    set(EXIFTOOL_BUNDLE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/exiftool/unix")
else()
    set(EXIFTOOL_BUNDLE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/exiftool/unix")
endif()

if(APPLE)
    set(EXIFTOOL_RUNTIME_DIR "$<TARGET_BUNDLE_DIR:rcaiman_projects>/Contents/Resources/exiftool")
else()
    set(EXIFTOOL_RUNTIME_DIR "$<TARGET_FILE_DIR:rcaiman_projects>/tools/exiftool")
endif()

add_custom_command(TARGET rcaiman_projects POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${EXIFTOOL_BUNDLE_SOURCE_DIR}"
        "${EXIFTOOL_RUNTIME_DIR}"
)
add_custom_command(TARGET rcaiman_projects POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "$<TARGET_FILE_DIR:rcaiman_projects>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/examples"
        "$<TARGET_FILE_DIR:rcaiman_projects>/examples"
)

if(WIN32)
    set(MSYS2_RUNTIME_DIR "C:/msys64/mingw64/bin")
    set(MSYS2_RUNTIME_DLL_NAMES
        libraw_r-25.dll
        libtiff-6.dll
        libdeflate.dll
        libjbig-0.dll
        libjpeg-8.dll
        libLerc.dll
        liblzma-5.dll
        libwebp-7.dll
        zlib1.dll
        libzstd.dll
        libgomp-1.dll
        liblcms2-2.dll
    )
    set(MSYS2_RUNTIME_DLLS)
    foreach(DLL_NAME IN LISTS MSYS2_RUNTIME_DLL_NAMES)
        list(APPEND MSYS2_RUNTIME_DLLS
            "${MSYS2_RUNTIME_DIR}/${DLL_NAME}")
        add_custom_command(TARGET rcaiman_projects POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MSYS2_RUNTIME_DIR}/${DLL_NAME}"
                "$<TARGET_FILE_DIR:rcaiman_projects>"
        )
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(metadataregistry_tests
        metadataregistry_tests.cpp
        metadataregistry.h metadataregistry.cpp
        collectionvalidation.h collectionvalidation.cpp
        project.h project.cpp
        acquisitionmetadata.h acquisitionmetadata.cpp
        collectionstylefile.h collectionstylefile.cpp
        hspprojectimporter.h hspprojectimporter.cpp
        pgmreader.h pgmreader.cpp
        rasterdata.h rasterdata.cpp
        rawreader.h rawreader.cpp
        tiffreader.h tiffreader.cpp
        tiffwriter.h tiffwriter.cpp
    )
    target_link_libraries(metadataregistry_tests PRIVATE
        Qt::Core
        Qt::Widgets
        TIFF
        LibRaw
        yaml-cpp
    )
    target_compile_definitions(metadataregistry_tests PRIVATE
        TEMPLATES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates"
    )
    add_custom_command(TARGET metadataregistry_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EXIFTOOL_BUNDLE_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:metadataregistry_tests>/tools/exiftool"
    )
    add_test(NAME metadataregistry_tests COMMAND metadataregistry_tests)
    if(WIN32)
        set_tests_properties(metadataregistry_tests PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${Qt6_DIR}/../../../bin;PATH=path_list_prepend:C:/Qt/Tools/mingw1310_64/bin;PATH=path_list_prepend:C:/msys64/mingw64/bin"
        )
    endif()
endif()

install(TARGETS rcaiman_projects
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(APPLE)
    install(DIRECTORY ${EXIFTOOL_BUNDLE_SOURCE_DIR}/
        DESTINATION rcaiman_projects.app/Contents/Resources/exiftool
    )
else()
    install(DIRECTORY ${EXIFTOOL_BUNDLE_SOURCE_DIR}/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/tools/exiftool
    )
endif()
install(DIRECTORY assets/examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(WIN32)
    install(FILES ${MSYS2_RUNTIME_DLLS}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

qt_generate_deploy_app_script(
    TARGET rcaiman_projects
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
