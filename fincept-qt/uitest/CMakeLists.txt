cmake_minimum_required(VERSION 3.20)
project(FinceptUITest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(THEMES
    theme01_obsidian
    theme02_matrix
    theme03_arctic
    theme04_carbon
    theme05_midnight
    theme06_neon
    theme07_slate
    theme08_bloomberg
    theme09_ember
    theme10_titanium
)

foreach(THEME ${THEMES})
    add_executable(${THEME} WIN32 ${THEME}.cpp)
    target_link_libraries(${THEME} PRIVATE Qt6::Widgets)
endforeach()
