cmake_minimum_required(VERSION3.25)

project(kasumi_win32_host VERSION0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Kasumi CONFIG REQUIRED)

add_executable(
    kasumi_win32_example
    WIN32
    main.cpp
)

target_link_libraries(
    kasumi_win32_example
    PRIVATE
        Kasumi::kasumi_windows_bridge
)

add_custom_command(
    TARGET kasumi_win32_example
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../examples/showcase"
        "$<TARGET_FILE_DIR:kasumi_win32_example>/web"
)

include(CTest)
if (BUILD_TESTING)
    add_test(
        NAME kasumi_win32_example_smoke
        COMMAND kasumi_win32_example --smoke-test
    )
    set_tests_properties(
        kasumi_win32_example_smoke
        PROPERTIES TIMEOUT 30
    )
endif()
