cmake_minimum_required(VERSION 3.15)
project(Neura)

set(CMAKE_CXX_STANDARD 17)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

set(WHISPER_DIR ${CMAKE_SOURCE_DIR}/whisper.cpp)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${WHISPER_DIR} whisper_build)

add_executable(Neura
    UI.cpp
    TimerAction.cpp
    SystemCommandAction.cpp
    VoiceCommandListener.cpp
)

target_include_directories(Neura PRIVATE
    /opt/homebrew/include
    ${WHISPER_DIR}/include
    ${WHISPER_DIR}/ggml/include
)

target_link_libraries(Neura
    SFML::Graphics
    SFML::Window
    SFML::System
    /opt/homebrew/lib/libportaudio.dylib
    whisper
    "-framework Cocoa"
    "-framework Accelerate"
)
