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
    src/platform/ksm_windows_host_services.cpp
)

target_include_directories(kasumi_windows_bridge PUBLIC src)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(kasumi_windows_bridge PUBLIC nlohmann_json::nlohmann_json)

if (WIN32)
    target_compile_definitions(kasumi_windows_bridge PUBLIC UNICODE _UNICODE)
    target_link_libraries(kasumi_windows_bridge PUBLIC comdlg32 shell32 user32)
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(kasumi_contract_runner tests/contract_runner.cpp)
    target_link_libraries(kasumi_contract_runner PRIVATE kasumi_windows_bridge)
    add_test(NAME kasumi_contract COMMAND kasumi_contract_runner)
endif()

install(TARGETS kasumi_windows_bridge ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(
    DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

set(CPACK_PACKAGE_NAME "Kasumi Windows Bridge")
set(CPACK_PACKAGE_VENDOR "Kasumi")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "kasumi-windows-bridge-${CPACK_PACKAGE_VERSION}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
