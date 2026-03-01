cmake_minimum_required(VERSION 3.13)
project(dubsiren VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# ALSA — required on target (Pi), optional for desktop compile-check
pkg_check_modules(ALSA alsa)

# libgpiod v2 — optional (not present on desktop)
pkg_check_modules(GPIOD libgpiod>=2.0)

set(SOURCES
    src/audio_engine.cpp
    src/delay.cpp
    src/filter.cpp
    src/gpio_hw.cpp
    src/lfo.cpp
    src/main.cpp
    src/oscillator.cpp
    src/reverb.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
)

# ── Optional: ALSA ──────────────────────────────────────────────────
if(ALSA_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_ALSA=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
else()
    message(STATUS "libasound not found — ALSA disabled (audio won't work)")
endif()

# ── Optional: libgpiod ──────────────────────────────────────────────
if(GPIOD_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_GPIOD=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${GPIOD_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GPIOD_LIBRARIES})
else()
    message(STATUS "libgpiod not found — GPIO disabled, --simulate forced")
endif()

# ── Optional: ws2811 (LED — not used until a later milestone) ───────
find_library(WS2811_LIB ws2811)
if(WS2811_LIB)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_WS2811=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${WS2811_LIB})
else()
    message(STATUS "libws2811 not found — LED support disabled")
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
