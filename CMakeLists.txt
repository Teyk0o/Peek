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
)

add_executable(Peek WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(Peek
    ws2_32   # Winsock
    iphlpapi # IP Helper API
    comctl32 # Common Controls
    uxtheme  # For visuals themes
    dwmapi   # Desktop Window Manager, Dark Mode
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