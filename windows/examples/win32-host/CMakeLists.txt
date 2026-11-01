cmake_minimum_required(VERSION 3.25)

project(kasumi_win32_host VERSION 26.11.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
    KASUMI_APP_PACKAGE_NAME
    "kasumi-win32-example"
    CACHE STRING
    "Application package name"
)
set(
    KASUMI_APP_VERSION
    "26.11.2-alpha"
    CACHE STRING
    "Application semantic version"
)
set(
    KASUMI_APP_COMMANDS
    ""
    CACHE STRING
    "Comma-separated application command allowlist"
)

if(NOT KASUMI_APP_PACKAGE_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9._-]*$")
    message(FATAL_ERROR "KASUMI_APP_PACKAGE_NAME has an invalid format.")
endif()
if(
    NOT KASUMI_APP_VERSION MATCHES
    "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?(\\+[0-9A-Za-z.-]+)?$"
)
    message(FATAL_ERROR "KASUMI_APP_VERSION must be a semantic version.")
endif()

option(
    KASUMI_EXAMPLE_EXTERNAL_WEB
    "Copy the showcase web assets beside the executable instead of embedding them"
    OFF
)

option(
    KASUMI_EXAMPLE_MULTI_VIEW
    "Start the showcase with two WebViews in one window"
    OFF
)

option(
    KASUMI_BUNDLE_WEBVIEW2_LOADER
    "Bundle WebView2Loader.dll instead of statically linking the loader"
    OFF
)

option(
    KASUMI_WINDOW_DECORATIONS
    "Show the native Windows title bar"
    ON
)

find_package(Kasumi CONFIG REQUIRED)

add_executable(
    kasumi_win32_example
    WIN32
    main.cpp
)

target_compile_features(kasumi_win32_example PRIVATE cxx_std_20)

get_target_property(
    KASUMI_WEBVIEW2_LOADER_TYPE
    unofficial::webview2::webview2
    TYPE
)
if(KASUMI_BUNDLE_WEBVIEW2_LOADER)
    if(NOT KASUMI_WEBVIEW2_LOADER_TYPE STREQUAL "SHARED_LIBRARY")
        message(
            FATAL_ERROR
            "KASUMI_BUNDLE_WEBVIEW2_LOADER=ON requires the vcpkg "
            "x64-windows triplet."
        )
    endif()
    get_target_property(
        KASUMI_WEBVIEW2_LOADER_DLL
        unofficial::webview2::webview2
        IMPORTED_LOCATION
    )
    add_custom_command(
        TARGET kasumi_win32_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KASUMI_WEBVIEW2_LOADER_DLL}"
            "$<TARGET_FILE_DIR:kasumi_win32_example>/WebView2Loader.dll"
    )
else()
    if(NOT KASUMI_WEBVIEW2_LOADER_TYPE STREQUAL "STATIC_LIBRARY")
        message(
            FATAL_ERROR
            "System WebView2 mode requires the vcpkg "
            "x64-windows-static-md triplet. Pass "
            "-DKASUMI_BUNDLE_WEBVIEW2_LOADER=ON to bundle the loader DLL."
        )
    endif()
    add_custom_command(
        TARGET kasumi_win32_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:kasumi_win32_example>/WebView2Loader.dll"
    )
endif()

set(
    KASUMI_EXAMPLE_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
file(MAKE_DIRECTORY "${KASUMI_EXAMPLE_GENERATED_DIR}")
string(
    REGEX MATCH
    "^([0-9]+)\\.([0-9]+)\\.([0-9]+)"
    KASUMI_APP_VERSION_NUMERIC
    "${KASUMI_APP_VERSION}"
)
set(KASUMI_APP_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(KASUMI_APP_VERSION_MINOR "${CMAKE_MATCH_2}")
set(KASUMI_APP_VERSION_PATCH "${CMAKE_MATCH_3}")
set(
    KASUMI_APP_VERSION_RC
    "${KASUMI_EXAMPLE_GENERATED_DIR}/app_version.rc"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/app_version.rc.in"
    "${KASUMI_APP_VERSION_RC}"
    @ONLY
)
target_sources(
    kasumi_win32_example
    PRIVATE
        "${KASUMI_APP_VERSION_RC}"
)
target_compile_definitions(
    kasumi_win32_example
    PRIVATE
        KASUMI_APP_PACKAGE_NAME="${KASUMI_APP_PACKAGE_NAME}"
        KASUMI_APP_VERSION="${KASUMI_APP_VERSION}"
        KASUMI_APP_COMMANDS="${KASUMI_APP_COMMANDS}"
        KASUMI_WINDOW_DECORATIONS=$<BOOL:${KASUMI_WINDOW_DECORATIONS}>
        KASUMI_EXAMPLE_MULTI_VIEW=$<BOOL:${KASUMI_EXAMPLE_MULTI_VIEW}>
)

set(
    KASUMI_EXAMPLE_WEB_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../examples/showcase"
)

if(KASUMI_EXAMPLE_EXTERNAL_WEB)
    target_compile_definitions(
        kasumi_win32_example
        PRIVATE
            KASUMI_EXAMPLE_EXTERNAL_WEB=1
    )
    add_custom_command(
        TARGET kasumi_win32_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${KASUMI_EXAMPLE_WEB_ROOT}"
            "$<TARGET_FILE_DIR:kasumi_win32_example>/web"
    )
else()
    file(
        GLOB_RECURSE KASUMI_EXAMPLE_WEB_FILES
        RELATIVE "${KASUMI_EXAMPLE_WEB_ROOT}"
        CONFIGURE_DEPENDS
        "${KASUMI_EXAMPLE_WEB_ROOT}/*"
    )
    list(FILTER KASUMI_EXAMPLE_WEB_FILES EXCLUDE REGEX "README\\.md$")
    list(SORT KASUMI_EXAMPLE_WEB_FILES)

    set(KASUMI_EXAMPLE_WEB_DEPENDS)
    foreach(relative_path IN LISTS KASUMI_EXAMPLE_WEB_FILES)
        list(
            APPEND KASUMI_EXAMPLE_WEB_DEPENDS
            "${KASUMI_EXAMPLE_WEB_ROOT}/${relative_path}"
        )
    endforeach()

    set(
        KASUMI_EXAMPLE_RESOURCE_RC
        "${KASUMI_EXAMPLE_GENERATED_DIR}/web_resources.rc"
    )
    set(
        KASUMI_EXAMPLE_RESOURCE_HEADER
        "${KASUMI_EXAMPLE_GENERATED_DIR}/web_resources.h"
    )
    add_custom_command(
        OUTPUT
            "${KASUMI_EXAMPLE_RESOURCE_RC}"
            "${KASUMI_EXAMPLE_RESOURCE_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            "-DWEB_ROOT=${KASUMI_EXAMPLE_WEB_ROOT}"
            "-DOUTPUT_RC=${KASUMI_EXAMPLE_RESOURCE_RC}"
            "-DOUTPUT_HEADER=${KASUMI_EXAMPLE_RESOURCE_HEADER}"
            "-DFILES=${KASUMI_EXAMPLE_WEB_FILES}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_web.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_web.cmake"
            ${KASUMI_EXAMPLE_WEB_DEPENDS}
        VERBATIM
    )

    target_include_directories(
        kasumi_win32_example
        PRIVATE
            "${KASUMI_EXAMPLE_GENERATED_DIR}"
    )
    target_sources(
        kasumi_win32_example
        PRIVATE
            "${KASUMI_EXAMPLE_RESOURCE_RC}"
            "${KASUMI_EXAMPLE_RESOURCE_HEADER}"
    )
    add_custom_command(
        TARGET kasumi_win32_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_FILE_DIR:kasumi_win32_example>/web"
    )
endif()

target_link_libraries(
    kasumi_win32_example
    PRIVATE
        Kasumi::kasumi_windows_bridge
)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME kasumi_win32_example_smoke
        COMMAND kasumi_win32_example --smoke-test
    )
    set_tests_properties(
        kasumi_win32_example_smoke
        PROPERTIES TIMEOUT 30
    )
    add_test(
        NAME kasumi_win32_example_multi_view_smoke
        COMMAND kasumi_win32_example --multi-view-smoke
    )
    set_tests_properties(
        kasumi_win32_example_multi_view_smoke
        PROPERTIES TIMEOUT 30
    )
endif()
