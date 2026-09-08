cmake_minimum_required(VERSION 3.20)

project(huxerui_mediaplayer VERSION 0.1.0 LANGUAGES NONE)

include("${CMAKE_CURRENT_SOURCE_DIR}/HuxerUIProject.cmake")
if (HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif ()

if (HUXERUI_LIBRARY_GRAPH_ONLY)
    add_library(huxerui_mediaplayer INTERFACE)
    set_target_properties(huxerui_mediaplayer PROPERTIES
            HUXERUI_LIBRARY TRUE
            HUXERUI_LIBRARY_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_library(HuxerUI::MediaPlayer ALIAS huxerui_mediaplayer)
    return()
endif ()

file(GLOB_RECURSE LIBRARY_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)
if (ANDROID)
    file(GLOB_RECURSE LIBRARY_ANDROID_SOURCE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/android/src/main/cpp/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/android/src/main/cpp/*.cc"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/android/src/main/cpp/*.cxx"
    )
    list(APPEND LIBRARY_SOURCE_FILES ${LIBRARY_ANDROID_SOURCE_FILES})
endif ()
if (WIN32)
    file(GLOB_RECURSE LIBRARY_WINDOWS_SOURCE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/src/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/src/*.cc"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/src/*.cxx"
    )
    list(APPEND LIBRARY_SOURCE_FILES ${LIBRARY_WINDOWS_SOURCE_FILES})
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(GLOB_RECURSE LIBRARY_LINUX_SOURCE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/src/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/src/*.cc"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/src/*.cxx"
    )
    list(APPEND LIBRARY_SOURCE_FILES ${LIBRARY_LINUX_SOURCE_FILES})
endif ()
if (APPLE)
    if (IOS)
        set(LIBRARY_OBJCXX_PLATFORM ios)
    else ()
        set(LIBRARY_OBJCXX_PLATFORM macos)
    endif ()
    file(GLOB_RECURSE LIBRARY_OBJCXX_SOURCE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/${LIBRARY_OBJCXX_PLATFORM}/src/*.mm"
    )
    set_source_files_properties(${LIBRARY_OBJCXX_SOURCE_FILES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    list(APPEND LIBRARY_SOURCE_FILES ${LIBRARY_OBJCXX_SOURCE_FILES})
endif ()
if (EMSCRIPTEN)
    file(GLOB_RECURSE LIBRARY_WEB_SOURCE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/web/src/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/web/src/*.cc"
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/web/src/*.cxx"
    )
    list(APPEND LIBRARY_SOURCE_FILES ${LIBRARY_WEB_SOURCE_FILES})
endif ()

huxerui_add_library(huxerui_mediaplayer
        SOURCES
            ${LIBRARY_SOURCE_FILES}
        RESOURCES
            resources
        RESOURCE_NAMESPACE
            media
)

target_include_directories(huxerui_mediaplayer PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_include_directories(huxerui_mediaplayer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if (WIN32)
    target_compile_definitions(huxerui_mediaplayer PRIVATE NOMINMAX UNICODE _UNICODE)
    target_link_libraries(huxerui_mediaplayer PRIVATE mfplat mfuuid mf d3d11 dxgi oleaut32)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MEDIAPLAYER_GTK REQUIRED IMPORTED_TARGET "gtk4>=4.14")
    target_link_libraries(huxerui_mediaplayer PRIVATE PkgConfig::MEDIAPLAYER_GTK)
endif ()

if (EMSCRIPTEN)
    set_property(TARGET huxerui_mediaplayer APPEND PROPERTY INTERFACE_LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/web/mediaplayer.js")
    target_link_options(huxerui_mediaplayer INTERFACE
            "SHELL:--pre-js \"${CMAKE_CURRENT_SOURCE_DIR}/platform/web/mediaplayer.js\""
    )
endif ()

if (APPLE)
    target_link_libraries(huxerui_mediaplayer PRIVATE "-framework AVFoundation" "-framework QuartzCore" "-framework CoreMedia")
endif ()

option(HUXERUI_MEDIAPLAYER_BUILD_TESTS "Build MediaPlayer contract tests" OFF)
if (HUXERUI_MEDIAPLAYER_BUILD_TESTS AND NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    enable_testing()
    add_subdirectory(tests)
endif ()

add_library(HuxerUI::MediaPlayer ALIAS huxerui_mediaplayer)
