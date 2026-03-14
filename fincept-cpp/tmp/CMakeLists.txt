cmake_minimum_required(VERSION 3.20)
project(YogaPlayground LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies (same as parent project via vcpkg)
find_package(yoga CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Console-only playground
add_executable(yoga_playground yoga_playground.cpp)
target_link_libraries(yoga_playground PRIVATE yoga::yogacore)

# GUI playground
add_executable(yoga_gui yoga_gui.cpp)
target_link_libraries(yoga_gui PRIVATE
    yoga::yogacore
    imgui::imgui
    glfw
    OpenGL::GL
)

if(MSVC)
    target_compile_definitions(yoga_playground PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(yoga_gui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    target_link_libraries(yoga_gui PRIVATE ws2_32)
    target_compile_definitions(yoga_gui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
