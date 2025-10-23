cmake_minimum_required(VERSION 3.27)
project(Peek C CXX)

# Project-wide settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -municode")
endif()

# Load version from build-config.json
if(EXISTS "${CMAKE_SOURCE_DIR}/build-config.json")
    file(READ "${CMAKE_SOURCE_DIR}/build-config.json" BUILD_CONFIG_JSON)
    string(JSON PEEK_VERSION GET "${BUILD_CONFIG_JSON}" version)
    string(JSON PEEK_APP_NAME GET "${BUILD_CONFIG_JSON}" app_name)
    message(STATUS "Peek ${PEEK_VERSION} - ${PEEK_APP_NAME}")
else()
    set(PEEK_VERSION "2.0.0-alpha.1")
    set(PEEK_APP_NAME "Peek")
    message(WARNING "build-config.json not found, using default version")
endif()

# Define version macros for C/C++
add_compile_definitions(PEEK_VERSION="${PEEK_VERSION}")
add_compile_definitions(PEEK_APP_NAME="${PEEK_APP_NAME}")

# Include shared headers
include_directories("${CMAKE_SOURCE_DIR}/shared")

# Sub-projects
message(STATUS "Adding sub-projects...")

# UI Host (peek.exe) - Win32 + WebView2
add_subdirectory(ui)

# Daemon (peekd.exe) - Windows Service
add_subdirectory(daemon)

# Updater (updater.exe)
add_subdirectory(updater)

# Code signing configuration for Release builds
if(WIN32 AND CMAKE_BUILD_TYPE MATCHES Release)
    find_program(SIGNTOOL signtool PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/x64"
    )

    if(SIGNTOOL)
        message(STATUS "Code signing enabled (signtool found)")
    else()
        message(WARNING "signtool.exe not found - binaries will not be signed")
    endif()
endif()

# Custom build target for frontend
add_custom_target(frontend_build
    COMMAND npm --prefix "${CMAKE_SOURCE_DIR}/frontend" run build
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/frontend"
    COMMENT "Building React frontend..."
)

# Ensure frontend is built first
add_dependencies(peek frontend_build)

# Custom target for packaging
add_custom_target(package
    COMMAND node "${CMAKE_SOURCE_DIR}/scripts/package.js"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Packaging Peek for distribution..."
)

# Output summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Peek Build Configuration")
message(STATUS "========================================")
message(STATUS "Version: ${PEEK_VERSION}")
message(STATUS "Platform: Windows")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Targets: peek (UI), peekd (Daemon), updater")
message(STATUS "")
message(STATUS "Build with:")
message(STATUS "  cmake --build build --config Release")
message(STATUS "")
message(STATUS "Package with:")
message(STATUS "  npm run package")
message(STATUS "========================================")
message(STATUS "")
