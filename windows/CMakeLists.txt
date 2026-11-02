cmake_minimum_required(VERSION 3.25)

file(
    READ
    "${CMAKE_CURRENT_SOURCE_DIR}/../version.json"
    KASUMI_VERSION_METADATA
)
string(JSON KASUMI_WINDOWS_SDK_VERSION GET "${KASUMI_VERSION_METADATA}" version)
string(
    REGEX MATCH
    "^[0-9]+\\.[0-9]+\\.[0-9]+"
    KASUMI_PROJECT_VERSION
    "${KASUMI_WINDOWS_SDK_VERSION}"
)
if(NOT KASUMI_PROJECT_VERSION)
    message(FATAL_ERROR "version.json contains an invalid release version.")
endif()

project(kasumi_windows VERSION "${KASUMI_PROJECT_VERSION}" LANGUAGES CXX)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(
    src/platform/ksm_version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include/ksm_version.h"
    @ONLY
)

option(
    KASUMI_BUNDLE_WEBVIEW2_LOADER
    "Bundle WebView2Loader.dll instead of statically linking the loader"
    OFF
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(
    kasumi_windows_bridge
    STATIC
    src/bridge/ksm_command_registry.cpp
    src/bridge/ksm_router.cpp
    src/commands/ksm_phase1.cpp
    src/commands/ksm_filesystem.cpp
    src/platform/ksm_virtual_path.cpp
    src/platform/ksm_window_frame.cpp
    src/platform/ksm_webview_host.cpp
    src/platform/ksm_webview_security.cpp
    src/platform/ksm_url_policy.cpp
    src/platform/ksm_web_resource.cpp
    src/platform/ksm_webview2_host.cpp
    src/platform/ksm_win32_resource_provider.cpp
    src/platform/ksm_odbc_sql_driver.cpp
    src/platform/ksm_sql_driver.cpp
    src/platform/ksm_sqlite_driver.cpp
    src/platform/ksm_windows_host_services.cpp
    src/plugins/ksm_clipboard_plugin.cpp
    src/plugins/ksm_dialog_plugin.cpp
    src/plugins/ksm_filesystem_plugin.cpp
    src/plugins/ksm_http_plugin.cpp
    src/plugins/ksm_log_plugin.cpp
    src/plugins/ksm_notification_plugin.cpp
    src/plugins/ksm_os_plugin.cpp
    src/plugins/ksm_path_plugin.cpp
    src/plugins/ksm_plugin_registry.cpp
    src/plugins/ksm_shell_plugin.cpp
    src/plugins/ksm_sql_plugin.cpp
    src/plugins/ksm_store_plugin.cpp
    src/plugins/ksm_window_plugin.cpp
)

target_compile_features(kasumi_windows_bridge PUBLIC cxx_std_20)

target_include_directories(
    kasumi_windows_bridge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(
    kasumi_windows_bridge
    PUBLIC
        nlohmann_json::nlohmann_json
        unofficial::sqlite3::sqlite3
)

if (WIN32)
    target_compile_definitions(
        kasumi_windows_bridge
        PUBLIC
            UNICODE
            _UNICODE
    )

    find_package(unofficial-webview2 CONFIG REQUIRED)
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
    else()
        if(NOT KASUMI_WEBVIEW2_LOADER_TYPE STREQUAL "STATIC_LIBRARY")
            message(
                FATAL_ERROR
                "System WebView2 mode requires the vcpkg "
                "x64-windows-static-md triplet. Pass "
                "-DKASUMI_BUNDLE_WEBVIEW2_LOADER=ON to bundle the loader DLL."
            )
        endif()
    endif()
    target_link_libraries(
        kasumi_windows_bridge
        PUBLIC
            comdlg32
            comctl32
            odbc32
            ole32
            shell32
            shlwapi
            user32
            winhttp
            unofficial::webview2::webview2
    )
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(
        kasumi_contract_runner
        tests/contract_runner.cpp
    )
    target_link_libraries(
        kasumi_contract_runner
        PRIVATE
            kasumi_windows_bridge
    )
    add_test(
        NAME kasumi_contract
        COMMAND kasumi_contract_runner
    )
endif()

install(
    TARGETS kasumi_windows_bridge
    EXPORT kasumiWindowsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT kasumiWindowsTargets
    FILE kasumiWindowsTargets.cmake
    NAMESPACE Kasumi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Kasumi
)

install(
    DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kasumi
    FILES_MATCHING
    PATTERN "*.h"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/include/ksm_version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kasumi/platform
)

configure_package_config_file(
    cmake/KasumiConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/KasumiConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Kasumi
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/KasumiConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Kasumi
)

install(
    FILES README.md
    DESTINATION .
)

set(CPACK_PACKAGE_NAME "Kasumi Windows SDK")
set(CPACK_PACKAGE_VENDOR "Kasumi")
set(CPACK_PACKAGE_VERSION "${KASUMI_WINDOWS_SDK_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "kasumi-windows-sdk-${CPACK_PACKAGE_VERSION}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP")

include(CPack)
