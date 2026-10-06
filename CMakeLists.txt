cmake_minimum_required(VERSION 3.21)
project(marky LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# md4c options must be set BEFORE add_subdirectory
set(BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(libs/md4c)

add_executable(marky
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/app/Theme.h
    src/app/Theme.cpp
    src/markdown/AST.h
    src/markdown/Parser.h
    src/markdown/Parser.cpp
    src/render/Renderer.h
    src/render/Renderer.cpp
)
target_include_directories(marky PRIVATE src libs/md4c/md4c)
target_link_libraries(marky PRIVATE Qt6::Widgets md4c)