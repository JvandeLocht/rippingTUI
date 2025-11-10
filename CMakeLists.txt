cmake_minimum_required(VERSION 3.20)
project(bluray-ripper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find FTXUI library
find_package(ftxui REQUIRED)

# Source files
add_executable(bluray-ripper
    src/main.cpp
    src/disc_detector.cpp
    src/makemkv_wrapper.cpp
    src/handbrake_wrapper.cpp
    src/ui/main_ui.cpp
)

target_include_directories(bluray-ripper PRIVATE include)

target_link_libraries(bluray-ripper
    PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# Enable warnings
target_compile_options(bluray-ripper PRIVATE
    -Wall -Wextra -Wpedantic
)

# Install the binary
install(TARGETS bluray-ripper DESTINATION bin)
