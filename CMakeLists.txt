cmake_minimum_required(VERSION 3.19)
project(rcaiman_projects LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

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
    resources.qrc
)

target_compile_definitions(rcaiman_projects PRIVATE
    TEMPLATES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates"
)

target_include_directories(rcaiman_projects
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

qt_add_translations(
    TARGETS rcaiman_projects
    TS_FILES rcaiman_projects_es_AR.ts
)

target_link_libraries(rcaiman_projects
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS rcaiman_projects
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY templates/ DESTINATION ${CMAKE_INSTALL_BINDIR}/templates)

qt_generate_deploy_app_script(
    TARGET rcaiman_projects
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
