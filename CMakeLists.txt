cmake_minimum_required(VERSION 4.1.2)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_PROCESSOR "armv6k")
set(DEVKITPRO "$ENV{DEVKITPRO}")
set(LIBCTRU "${DEVKITPRO}/libctru")
set(PORTLIBS_PATH "${DEVKITPRO}/portlibs")
set(PORTLIBS "${PORTLIBS_PATH}/3ds")

list(APPEND CMAKE_PROGRAM_PATH  "${DEVKITPRO}/tools/bin")
list(APPEND CMAKE_PROGRAM_PATH  "${DEVKITPRO}/devkitARM/bin")
set(CMAKE_C_COMPILER "arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-none-eabi-g++")

project(ModPlayer3DS)

set(APP_TITLE "ModPlayer3DS")
set(APP_AUTHOR "ameOverflow")
set(APP_VERSION "0.3")
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/meta/music-player.png")

set(SOURCES
        source/main.cpp
        source/Playback.cpp
)

add_executable(ModPlayer3DS ${SOURCES})
set_target_properties(ModPlayer3DS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

set(ARCH -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft)

target_include_directories(ModPlayer3DS PRIVATE ${LIBCTRU}/include ${PORTLIBS}/include ./source ./include)
target_link_directories(ModPlayer3DS PRIVATE ${LIBCTRU}/lib ${PORTLIBS}/lib ./lib)
target_link_libraries(ModPlayer3DS PRIVATE openmpt citro2d citro3d ctru m)

target_compile_options(ModPlayer3DS PRIVATE
        ${ARCH}
        -mword-relocations
        -ffunction-sections
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti;-fexceptions>
)

target_compile_definitions(ModPlayer3DS PRIVATE
        __3DS__
)

target_link_options(ModPlayer3DS PRIVATE
        ${ARCH}
        --specs=3dsx.specs
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(ModPlayer3DS PRIVATE -O3)
    target_link_options(ModPlayer3DS PRIVATE -O3)
else()
    target_compile_options(ModPlayer3DS PRIVATE -g3 -O0)
    target_link_options(ModPlayer3DS PRIVATE -g3 -O0 -Wl,-Map,${TARGET}.map)
endif()


add_custom_command(TARGET ModPlayer3DS POST_BUILD
        COMMAND smdhtool --create "${APP_TITLE}" "${APP_TITLE}" "${APP_AUTHOR}" "${APP_ICON}" ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh
        COMMAND 3dsxtool $<TARGET_FILE:ModPlayer3DS> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.3dsx --smdh=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh
        COMMAND bannertool makebanner -ci "${CMAKE_CURRENT_SOURCE_DIR}/meta/boing.cgfx" -ca "${CMAKE_CURRENT_SOURCE_DIR}/meta/banner.bcwav" -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bnr"
        COMMAND makerom -f cia -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.cia -target t -exefslogo -elf $<TARGET_FILE:ModPlayer3DS> -rsf "${CMAKE_CURRENT_SOURCE_DIR}/meta/modplayer3ds.rsf" -banner "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bnr" -icon "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh" -ver 48
        COMMENT "Building 3DSX and CIA"
)