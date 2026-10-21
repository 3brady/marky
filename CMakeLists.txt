cmake_minimum_required(VERSION 3.21)
project(marky LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(libs/md4c)

add_library(marky_markdown STATIC
    src/markdown/AST.h
    src/markdown/Parser.h
    src/markdown/Parser.cpp
)
target_include_directories(marky_markdown
    PUBLIC src
    PRIVATE libs/md4c/md4c
)
target_link_libraries(marky_markdown PUBLIC md4c)

add_executable(marky
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/style/Theme.h
    src/style/Theme.cpp
    src/render/Renderer.h
    src/render/Renderer.cpp
    src/style/Highlighter.cpp
    src/style/Highlighter.h
)
target_link_libraries(marky PRIVATE Qt6::Widgets marky_markdown)

enable_testing()
add_executable(markdown_tests tests/markdown_tests.cpp)
target_link_libraries(markdown_tests PRIVATE marky_markdown)
add_test(NAME markdown COMMAND markdown_tests)
