cmake_minimum_required(VERSION 3.13)
project(dubsiren VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    asound
    gpiod
    ws2811
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
