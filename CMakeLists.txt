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
set(ARCH
        -march=armv6k
        -mtune=mpcore
        -mfloat-abi=hard
        -mtp=soft
)

project(ModPlayer3DS)

set(APP_TITLE "ModPlayer3DS")
set(APP_AUTHOR "ameOverflow")
set(APP_VERSION "0.6")
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/meta/music-player.png")
set(GRAPHICS "${CMAKE_CURRENT_SOURCE_DIR}/gfx")
set(ROMFS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(GFXBUILD "${CMAKE_CURRENT_SOURCE_DIR}/assets/gfx")

set(SOURCES
        source/main.cpp
        source/Playback.cpp
        source/Waveform.h
        source/waveforms/WaveformOld.cpp
        source/waveforms/WaveformOld.h
        source/waveforms/WaveformNew.cpp
        source/waveforms/WaveformNew.h
)

# compile t3s stuffs
file(GLOB T3S_FILES "${GRAPHICS}/*.t3s")
file(MAKE_DIRECTORY ${GFXBUILD})
set(TEX_OUTPUTS ""
        source/Theme.h)
message(STATUS "Found graphics files: ${T3S_FILES}")

foreach(T3S_FILE ${T3S_FILES})
    get_filename_component(FILE_NAME ${T3S_FILE} NAME_WE)
    add_custom_command(
            OUTPUT "${GFXBUILD}/${FILE_NAME}.t3x"
            DEPENDS ${T3S_FILE}
            COMMAND tex3ds -i ${T3S_FILE} -o "${GFXBUILD}/${FILE_NAME}.t3x"
            COMMENT "compiling ${FILE_NAME}.t3s"
    )
    list(APPEND TEX_OUTPUTS "${GFXBUILD}/${FILE_NAME}.t3x")
endforeach()

# compile libopenmpt from source
include(ExternalProject)

string(REPLACE ";" " " ARCH_STR "${ARCH}")
cmake_host_system_information(RESULT HOST_CPU_COUNT QUERY NUMBER_OF_LOGICAL_CORES)

ExternalProject_Add(libopenmpt_build
        PREFIX ${CMAKE_BINARY_DIR}/libopenmpt-ext
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/libopenmpt
        CONFIGURE_COMMAND ""
        BUILD_COMMAND sh -c "cd ${CMAKE_SOURCE_DIR}/ext/libopenmpt && make -j${HOST_CPU_COUNT} CONFIG=gcc \
              CC='${CMAKE_CXX_COMPILER}' \
              CXX='${CMAKE_CXX_COMPILER}' \
              AR='${CMAKE_AR}' \
              RANLIB='${CMAKE_RANLIB}' \
              PKG_CONFIG='${PORTLIBS}/bin/arm-none-eabi-pkg-config' \
              CFLAGS='${ARCH_STR}' \
              CXXFLAGS='${ARCH_STR}' \
              LDFLAGS='-L${PORTLIBS}/lib -lc -lgcc' \
              SHARED_LIB=0 \
              STATIC_LIB=1 \
              TEST=0 \
              EXAMPLES=0 \
              OPENMPT123=0 \
              NO_VORBIS=1 \
              NO_VORBISFILE=1"
        BUILD_IN_SOURCE 1
        USES_TERMINAL_BUILD 1
        INSTALL_COMMAND ""
)

set(LIBOPENMPT_ARCHIVE ${CMAKE_BINARY_DIR}/libopenmpt-ext/src/libopenmpt_build/bin/libopenmpt.a)

add_library(libopenmpt STATIC IMPORTED GLOBAL)
set_target_properties(libopenmpt PROPERTIES
        IMPORTED_LOCATION "${LIBOPENMPT_ARCHIVE}"
)

# set up executable
add_executable(ModPlayer3DS ${SOURCES} ${TEX_OUTPUTS})
set_target_properties(ModPlayer3DS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ModPlayer3DS PRIVATE ${LIBCTRU}/include ${PORTLIBS}/include "${CMAKE_SOURCE_DIR}/ext/libopenmpt" ./source)
target_link_directories(ModPlayer3DS PRIVATE ${LIBCTRU}/lib ${PORTLIBS}/lib ./lib)
target_link_libraries(ModPlayer3DS PRIVATE openmpt citro2d citro3d ctru m)
add_dependencies(ModPlayer3DS libopenmpt_build)

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

# build rom
add_custom_command(TARGET ModPlayer3DS POST_BUILD
        COMMAND smdhtool --create "${APP_TITLE}" "${APP_TITLE}" "${APP_AUTHOR}" "${APP_ICON}" ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh
        COMMAND 3dsxtool $<TARGET_FILE:ModPlayer3DS> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.3dsx --smdh=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh --romfs=${ROMFS}
        COMMAND bannertool makebanner -ci "${CMAKE_CURRENT_SOURCE_DIR}/meta/boing.cgfx" -ca "${CMAKE_CURRENT_SOURCE_DIR}/meta/banner.bcwav" -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bnr"
        COMMAND makerom -f cia -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.cia -target t -exefslogo -elf $<TARGET_FILE:ModPlayer3DS> -rsf "${CMAKE_CURRENT_SOURCE_DIR}/meta/modplayer3ds.rsf" -banner "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bnr" -icon "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh" -ver 96 -DAPP_ROMFS="${ROMFS}"
        COMMENT "Building 3DSX and CIA"
)