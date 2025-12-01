cmake_minimum_required(VERSION 3.28)
project(MinesweeperP3 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)


add_executable(out src/main.cpp)
target_compile_features(out PRIVATE cxx_std_17)
target_link_libraries(out PRIVATE SFML::Graphics)