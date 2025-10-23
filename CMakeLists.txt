cmake_minimum_required(VERSION 3.27)
project(Peek C)

set(CMAKE_C_STANDARD 11)

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -municode")
endif ()

set(SOURCES
    main.c
    network.c
    gui.c
    logger.c
)

set(HEADERS
    network.h
    gui.h
    logger.h
    resource.h
)

# Add Windows resource file for icon
if(WIN32)
    enable_language(RC)
    set(RESOURCES app.rc)
endif()

add_executable(Peek WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(Peek
    ws2_32    # Winsock
    iphlpapi  # IP Helper API
    comctl32  # Common Controls
    psapi     # Process Status API
    crypt32   # Cryptography API (DPAPI, SHA256, HMAC)
    wintrust  # WinVerifyTrust API
    shell32   # SHGetFolderPath
)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    if(MINGW)
        set_target_properties(Peek PROPERTIES
            LINK_FLAGS "-Wl,--subsystem,console"
        )
    else()
        set_target_properties(Peek PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()
endif ()

# Code signing configuration for Release builds
if(WIN32 AND CMAKE_BUILD_TYPE MATCHES Release)
    find_program(SIGNTOOL signtool PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/x64"
    )

    if(SIGNTOOL)
        # Sign the executable after build
        add_custom_command(TARGET Peek POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Signing Peek.exe with certificate..."
            COMMAND ${SIGNTOOL} sign /f "${CMAKE_SOURCE_DIR}/PeekCertificate.pfx"
                    /p "$ENV{PEEK_CERT_PASSWORD}"
                    /tr http://timestamp.digicert.com
                    /td sha256
                    /fd sha256
                    /v
                    "$<TARGET_FILE:Peek>"
            COMMENT "Code signing Peek.exe"
            VERBATIM
        )
        message(STATUS "Code signing enabled - using PeekCertificate.pfx")
    else()
        message(WARNING "signtool.exe not found - executable will not be signed")
    endif()
endif()