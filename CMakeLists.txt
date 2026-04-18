cmake_minimum_required(VERSION 3.16)
project(cloud_client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable(cloud_client
    src/main.cpp
    src/MainWindow.cpp
    src/AppController.cpp
    src/MockCloudProvider.cpp
    src/TaskManager.cpp
)

target_include_directories(cloud_client PRIVATE src)
target_link_libraries(cloud_client PRIVATE PkgConfig::GTK4)