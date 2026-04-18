cmake_minimum_required(VERSION 3.16)
project(cloud_client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(JSONGLIB REQUIRED IMPORTED_TARGET json-glib-1.0)
pkg_check_modules(LIBNOTIFY REQUIRED IMPORTED_TARGET libnotify)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)

add_executable(cloud_client
    src/main.cpp
    src/MainWindow.cpp
    src/AppController.cpp
    src/CacheManager.cpp
    src/SystemIntegration.cpp
    src/YandexDiskClient.cpp
    src/MetadataStore.cpp
    src/PendingOperationsStore.cpp
    src/NotificationManager.cpp
    src/ImagePreviewWindow.cpp
    src/ClipboardManager.cpp
    src/AppConfigStore.cpp
    src/YandexOAuthClient.cpp
)

target_include_directories(cloud_client PRIVATE src)

target_link_libraries(cloud_client PRIVATE
    PkgConfig::GTK4
    PkgConfig::JSONGLIB
    PkgConfig::LIBNOTIFY
    SQLite::SQLite3
    CURL::libcurl
)