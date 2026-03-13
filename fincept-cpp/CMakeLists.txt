cmake_minimum_required(VERSION 3.20)
project(FinceptTerminal VERSION 3.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Dependencies via vcpkg
# =============================================================================
find_package(glfw3 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(imgui CONFIG REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

# =============================================================================
# Sources
# =============================================================================
set(SOURCES
    src/main.cpp
    src/app.cpp
    src/http/http_client.cpp
    src/storage/sqlite_store.cpp
    src/storage/crypto.cpp
    src/auth/auth_api.cpp
    src/auth/auth_manager.cpp
    src/auth/login_screen.cpp
    src/auth/register_screen.cpp
    src/auth/forgot_password.cpp
    src/auth/pricing_screen.cpp
    src/dashboard/dashboard_screen.cpp
    src/dashboard/dashboard_data.cpp
    src/dashboard/stock_ticker.cpp
    src/profile/profile_screen.cpp
    src/news/news_screen.cpp
    src/research/research_data.cpp
    src/research/research_screen.cpp
    src/research/overview_panel.cpp
    src/research/financials_panel.cpp
    src/research/analysis_panel.cpp
    src/research/technicals_panel.cpp
    src/research/news_panel.cpp
    src/markets/markets_data.cpp
    src/markets/markets_screen.cpp
    src/utils/device_id.cpp
)

# =============================================================================
# Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    CURL::libcurl
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    OpenSSL::SSL
    OpenSSL::Crypto
    imgui::imgui
)

# =============================================================================
# Platform-specific
# =============================================================================
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 crypt32 wldap32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    # Use console subsystem for now (shows debug output)
    # Switch to WIN32_EXECUTABLE for production release to hide console
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        "-framework OpenGL"
    )
elseif(UNIX)
    # Linux
    find_package(PkgConfig REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        dl
    )
endif()

# =============================================================================
# Copy fonts to build directory
# =============================================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts
)

# =============================================================================
# Install rules (for packaging)
# =============================================================================
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fonts DESTINATION bin/fonts)
