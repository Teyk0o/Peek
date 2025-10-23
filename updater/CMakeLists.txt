cmake_minimum_required(VERSION 3.27)
project(Peek-Updater C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
    src/main.c
    src/downloader.c
    src/verifier.c
    src/installer.c
    src/logger.c
)

set(HEADERS
    src/downloader.h
    src/verifier.h
    src/installer.h
    src/logger.h
)

# Add Windows resource file
if(WIN32)
    enable_language(RC)
    list(APPEND SOURCES src/resource.rc)
endif()

add_executable(updater ${SOURCES} ${HEADERS})

# Target properties
set_target_properties(updater PROPERTIES
    VERSION ${PEEK_VERSION}
    SOVERSION 2
)

# Include directories
target_include_directories(updater PRIVATE
    "${CMAKE_SOURCE_DIR}/shared"
    "src"
)

# Link libraries
target_link_libraries(updater
    PRIVATE
    ws2_32
    wininet
    crypt32
    wintrust
    shell32
    psapi
    shlwapi
    advapi32
)

# Optimization flags
if(MSVC)
    target_compile_options(updater PRIVATE /W4 /WX)
else()
    target_compile_options(updater PRIVATE -Wall -Wextra -Werror)
endif()

# Code signing (if Release and signtool available)
if(SIGNTOOL AND CMAKE_BUILD_TYPE MATCHES Release)
    add_custom_command(TARGET updater POST_BUILD
        COMMAND ${SIGNTOOL} sign /f "${CMAKE_SOURCE_DIR}/PeekCertificate.pfx"
                /p "$ENV{PEEK_CERT_PASSWORD}"
                /tr http://timestamp.digicert.com
                /td sha256
                /fd sha256
                /v
                "$<TARGET_FILE:updater>"
        COMMENT "Code signing updater.exe"
        VERBATIM
    )
endif()

message(STATUS "Updater (updater.exe) - Configured")
message(STATUS "  Features: Secure download, RSA signature verification, atomic install")
message(STATUS "  Manifest: JSON with RSA-SHA256 signature")
