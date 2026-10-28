cmake_minimum_required(VERSION 3.25)

project(kasumi_windows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    target_link_libraries(kasumi_windows_bridge PUBLIC comdlg32 shell32 user32)
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(kasumi_contract_runner tests/contract_runner.cpp)
    target_link_libraries(kasumi_contract_runner PRIVATE kasumi_windows_bridge)
    add_test(NAME kasumi_contract COMMAND kasumi_contract_runner)
endif()
