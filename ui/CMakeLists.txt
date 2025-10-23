cmake_minimum_required(VERSION 3.27)
project(Peek-UI CXX)

set(CMAKE_CXX_STANDARD 20)

# WebView2 SDK
if(NOT DEFINED WEBVIEW2_INCLUDE_DIR)
    set(WEBVIEW2_INCLUDE_DIR "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/cppwinrt")
endif()

# Find WebView2 NuGet package
find_package(WebView2 QUIET)

if(NOT WebView2_FOUND)
    message(WARNING "WebView2 SDK not found. Please install WebView2 NuGet package:")
    message("  vcpkg install webview2:x64-windows")
    message("  OR manually set WEBVIEW2_INCLUDE_DIR and WEBVIEW2_LIB_DIR")
endif()

set(SOURCES
    src/main.cpp
    src/webview.cpp
    src/ipc-client.cpp
    src/window.cpp
)

set(HEADERS
    src/webview.h
    src/ipc-client.h
    src/window.h
)

# Add Windows resource file for icon/manifest
if(WIN32)
    enable_language(RC)
    list(APPEND SOURCES src/resource.rc)
endif()

add_executable(peek WIN32 ${SOURCES} ${HEADERS})

# Target properties
set_target_properties(peek PROPERTIES
    VERSION ${PEEK_VERSION}
    SOVERSION 2
    WIN32_EXECUTABLE ON
)

# Include directories
target_include_directories(peek PRIVATE
    "${CMAKE_SOURCE_DIR}/shared"
    "src"
    ${WEBVIEW2_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(peek
    PRIVATE
    ws2_32
    iphlpapi
    comctl32
    shell32
    psapi
    crypt32
    wintrust
)

# WebView2 linking
if(WebView2_FOUND)
    target_link_libraries(peek PRIVATE WebView2::WebView2)
else()
    message(WARNING "WebView2 not linked - runtime dependency required")
endif()

# Optimization flags
if(MSVC)
    target_compile_options(peek PRIVATE /W4 /WX)
else()
    target_compile_options(peek PRIVATE -Wall -Wextra -Werror)
endif()

# Copy frontend build artifacts
add_custom_command(TARGET peek POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/resources/www"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/frontend/dist"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/www"
    COMMENT "Copying frontend assets to UI host..."
)

# Code signing (if Release and signtool available)
if(SIGNTOOL AND CMAKE_BUILD_TYPE MATCHES Release)
    add_custom_command(TARGET peek POST_BUILD
        COMMAND ${SIGNTOOL} sign /f "${CMAKE_SOURCE_DIR}/PeekCertificate.pfx"
                /p "$ENV{PEEK_CERT_PASSWORD}"
                /tr http://timestamp.digicert.com
                /td sha256
                /fd sha256
                /v
                "$<TARGET_FILE:peek>"
        COMMENT "Code signing peek.exe"
        VERBATIM
    )
endif()

message(STATUS "UI Host (peek.exe) - Configured")
message(STATUS "  WebView2: ${WebView2_FOUND}")
message(STATUS "  Frontend assets: frontend/dist -> ui/resources/www")
