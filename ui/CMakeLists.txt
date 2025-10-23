cmake_minimum_required(VERSION 3.27)
project(Peek-UI CXX)

set(CMAKE_CXX_STANDARD 20)

# WebView2 SDK - Use vcpkg's unofficial-webview2
find_package(unofficial-webview2 CONFIG QUIET)

if(NOT unofficial-webview2_FOUND)
    # Fallback to manual WebView2 detection
    if(NOT DEFINED WEBVIEW2_INCLUDE_DIR)
        set(WEBVIEW2_INCLUDE_DIR "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/cppwinrt")
    endif()

    message(WARNING "WebView2 SDK not found via vcpkg. Please install WebView2 NuGet package:")
    message("  vcpkg install webview2:x64-windows")
    message("  OR manually set WEBVIEW2_INCLUDE_DIR and WEBVIEW2_LIB_DIR")
    set(WebView2_FOUND FALSE)
else()
    set(WebView2_FOUND TRUE)
    message(STATUS "WebView2 found via vcpkg")
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
    LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/manifest.xml"
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
    target_link_libraries(peek PRIVATE unofficial::webview2::webview2)
    message(STATUS "Linking WebView2 from vcpkg")
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
