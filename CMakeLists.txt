cmake_minimum_required(VERSION 3.21)
project(typefast LANGUAGES CXX)

add_executable(typefast WIN32
    src/main.cpp
    src/app.cpp
    src/game.cpp
)

target_compile_features(typefast PRIVATE cxx_std_17)
target_compile_definitions(typefast PRIVATE
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)
target_link_libraries(typefast PRIVATE d2d1 dwrite shell32 ole32)

# Design goal: tiny self-contained exe. Static CRT, optimize for size.
set_property(TARGET typefast PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
    target_compile_options(typefast PRIVATE /utf-8 /W4 $<$<CONFIG:Release>:/O1 /GL>)
    target_link_options(typefast PRIVATE $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
endif()
