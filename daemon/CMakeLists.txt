cmake_minimum_required(VERSION 3.27)
project(Peek-Daemon C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
    src/main.c
    src/service.c
    src/ipc.c
    src/network.c
    src/logger.c
    src/config.c
    src/auto-update.c
)

set(HEADERS
    src/service.h
    src/ipc.h
    src/network.h
    src/logger.h
    src/config.h
    src/auto-update.h
)

# Add Windows resource file
if(WIN32)
    enable_language(RC)
    list(APPEND SOURCES src/resource.rc)
endif()

add_executable(peekd ${SOURCES} ${HEADERS})

# Target properties
set_target_properties(peekd PROPERTIES
    VERSION ${PEEK_VERSION}
    SOVERSION 2
)

# Include directories
target_include_directories(peekd PRIVATE
    "${CMAKE_SOURCE_DIR}/shared"
    "src"
)

# Link libraries
target_link_libraries(peekd
    PRIVATE
    ws2_32
    iphlpapi
    comctl32
    shell32
    psapi
    crypt32
    wintrust
    advapi32
    shlwapi
)

# Optimization flags
if(MSVC)
    target_compile_options(peekd PRIVATE /W4 /WX)
else()
    target_compile_options(peekd PRIVATE -Wall -Wextra -Werror)
endif()

# Code signing (if Release and signtool available)
if(SIGNTOOL AND CMAKE_BUILD_TYPE MATCHES Release)
    add_custom_command(TARGET peekd POST_BUILD
        COMMAND ${SIGNTOOL} sign /f "${CMAKE_SOURCE_DIR}/PeekCertificate.pfx"
                /p "$ENV{PEEK_CERT_PASSWORD}"
                /tr http://timestamp.digicert.com
                /td sha256
                /fd sha256
                /v
                "$<TARGET_FILE:peekd>"
        COMMENT "Code signing peekd.exe"
        VERBATIM
    )
endif()

message(STATUS "Daemon (peekd.exe) - Configured")
message(STATUS "  Service: Windows Service with IPC")
message(STATUS "  IPC Protocol: Named Pipes (\\\\?\\pipe\\Peek-IPC-v1)")
