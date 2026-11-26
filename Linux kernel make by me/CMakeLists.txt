# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(Clolix CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Clolix WIN32
        main.cpp
        vim.cpp
        browser.cpp
        backendopt.cpp
        compiler.cpp
)

target_link_libraries(Clolix PRIVATE
        user32
        gdi32
        kernel32
        advapi32
        shell32
        comctl32
        comdlg32
)