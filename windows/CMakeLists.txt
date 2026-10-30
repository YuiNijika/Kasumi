cmake_minimum_required(VERSION 3.25)

project(kasumi_windows VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_INCLUDEDIR include)
set(CMAKE_INSTALL_LIBDIR lib)

add_library(kasumi_windows_bridge STATIC
    src/bridge/ksm_command_registry.cpp
    src/bridge/ksm_router.cpp
    src/commands/ksm_phase1.cpp
    src/commands/ksm_filesystem.cpp
    src/platform/ksm_virtual_path.cpp
    src/platform/ksm_webview_host.cpp
    src/platform/ksm_webview_security.cpp
    src/platform/ksm_url_policy.cpp
    src/platform/ksm_webview2_host.cpp
    src/platform/ksm_windows_host_services.cpp
)

target_include_directories(kasumi_windows_bridge PUBLIC src)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(kasumi_windows_bridge PUBLIC nlohmann_json::nlohmann_json)

if (WIN32)
    target_compile_definitions(kasumi_windows_bridge PUBLIC UNICODE _UNICODE)
    find_package(unofficial-webview2 CONFIG REQUIRED)
    target_link_libraries(
        kasumi_windows_bridge
        PUBLIC
            comdlg32
            comctl32
            ole32
            shell32
            user32
            winhttp
            unofficial::webview2::webview2
    )

    add_executable(
        kasumi_win32_example
        WIN32
        examples/win32-host/main.cpp
    )
    target_link_libraries(
        kasumi_win32_example
        PRIVATE
            kasumi_windows_bridge
            comctl32
            ole32
            shell32
            unofficial::webview2::webview2
    )
    target_compile_definitions(kasumi_win32_example PRIVATE UNICODE _UNICODE)
    add_custom_command(
        TARGET kasumi_win32_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/win32-host/web"
            "$<TARGET_FILE_DIR:kasumi_win32_example>/web"
    )
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(kasumi_contract_runner tests/contract_runner.cpp)
    target_link_libraries(kasumi_contract_runner PRIVATE kasumi_windows_bridge)
    add_test(NAME kasumi_contract COMMAND kasumi_contract_runner)
    if (WIN32)
        add_test(
            NAME kasumi_win32_example_smoke
            COMMAND kasumi_win32_example --smoke-test
        )
        set_tests_properties(
            kasumi_win32_example_smoke
            PROPERTIES TIMEOUT 30
        )
    endif()
endif()

install(TARGETS kasumi_windows_bridge ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(
    DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
if (WIN32)
    install(TARGETS kasumi_win32_example RUNTIME DESTINATION .)
    install(
        DIRECTORY examples/win32-host/web
        DESTINATION .
    )
    install(
        FILES examples/win32-host/README.md
        DESTINATION .
        RENAME EXAMPLE_HOST.md
    )
endif()
install(FILES README.md DESTINATION .)

set(CPACK_PACKAGE_NAME "Kasumi Windows Bridge")
set(CPACK_PACKAGE_VENDOR "Kasumi")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "kasumi-windows-bridge-${CPACK_PACKAGE_VERSION}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
