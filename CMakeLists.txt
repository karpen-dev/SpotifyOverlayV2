cmake_minimum_required(VERSION 3.16)
project(SpotifyOverlay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(PkgConfig REQUIRED)
find_package(cpprestsdk REQUIRED)
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

include_directories(include)

set(HEADERS
        include/TrackOverlay.h
        include/AuthManager.h
        include/ConfigManager.h
        include/SpotifyAPI.h
        include/Types.h
)

set(SOURCES
        src/ConfigManager.cpp
        src/AuthManager.cpp
        src/SpotifyAPI.cpp
        src/TrackOverlay.cpp
        src/main.cpp
)

add_executable(SpotifyOverlay ${SOURCES} ${HEADERS})

target_link_libraries(SpotifyOverlay
        PRIVATE
        cpprestsdk::cpprest
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
)