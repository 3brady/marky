cmake_minimum_required(VERSION 3.21)
project(mdedit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(mdedit
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
)
target_link_libraries(mdedit PRIVATE Qt6::Widgets)
