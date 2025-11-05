cmake_minimum_required(VERSION 3.16)
project(url_shortener CXX)
set(CMAKE_CXX_STANDARD 20)

# Generate compile commands for VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Drogon CONFIG REQUIRED) # install Drogon first (see below)

add_executable(url_shortener src/main.cpp)
target_link_libraries(url_shortener PRIVATE Drogon::Drogon)
