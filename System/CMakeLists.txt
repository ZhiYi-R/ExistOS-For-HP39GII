

include_directories(./)
include_directories(./Core)
include_directories(./Drivers)
include_directories(./UI)
include_directories(./UI/Core)
include_directories(./UI/Screens)
include_directories(${CMAKE_SOURCE_DIR}/fonts)
include_directories(./UI/Lang)
include_directories(./Utils)
include_directories(./Fs)

include_directories(${CMAKE_SOURCE_DIR}/External/FreeRTOS/System/core)
include_directories(${CMAKE_SOURCE_DIR}/External/FreeRTOS/System/port)
include_directories(${CMAKE_SOURCE_DIR}/External/FreeRTOS/System/include)

include_directories(${CMAKE_SOURCE_DIR}/External/FatFs)
include_directories(${CMAKE_SOURCE_DIR}/External/LibTomMath/libtommath-0.39/)
include_directories(${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/)
include_directories(./Apps/user/khicas)
include_directories(${CMAKE_SOURCE_DIR}/External/GMP/include/)


aux_source_directory(./Core                                          SYS_SRCS)
aux_source_directory(./Drivers                                       SYS_SRCS)
aux_source_directory(./UI                                            SYS_SRCS)
aux_source_directory(./UI/Core                                       SYS_SRCS)
aux_source_directory(./UI/Screens                                    SYS_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/fonts                                      SYS_SRCS)
aux_source_directory(./UI/Lang                                       SYS_SRCS)
aux_source_directory(./Utils                                         SYS_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/External/FreeRTOS/System/core                SYS_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/External/FreeRTOS/System/port                SYS_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/External/FreeRTOS/System/memory              SYS_SRCS)
aux_source_directory(./Fs                                           SYS_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/External/FatFs                              SYS_SRCS)
aux_source_directory(./Crash                                         SYS_SRCS)

# 添加getentropy.c文件到编译源文件
list(APPEND SYS_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/Core/getentropy.cpp)

add_subdirectory(${CMAKE_SOURCE_DIR}/External/TJpgDec tjpgd)
add_subdirectory(Apps/user/khicas)

add_executable(sys.elf ${SYS_SRCS})

# --- Generate the compact CJK glyph table from UI_Chinese.h at build time -----
# draw_text decodes UTF-8 per character and looks each code point up in a
# compile-time hash table built over this generated header. gen_cjk_font.py scans
# the UTF-8 source for CJK code points and rasterizes each from GNU Unifont (the
# same glyphs as the retired fonts_hzk blob), so on-screen Chinese is byte-identical
# while the checked-in source stays UTF-8 (no GBK transcoding, no full font blob).
find_program(UV uv)
if(NOT UV)
  message(FATAL_ERROR "uv not found: it is required to generate the CJK glyph table from UI_Chinese.h. Install uv or add it to PATH.")
endif()

set(CJK_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/cjk_gen)
add_custom_command(
  OUTPUT ${CJK_GEN_DIR}/cjk_font_data.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CJK_GEN_DIR}
  COMMAND ${UV} run -q --project ${CMAKE_SOURCE_DIR}/Scripts python
          ${CMAKE_SOURCE_DIR}/Scripts/gen_cjk_font.py
          --source ${CMAKE_CURRENT_SOURCE_DIR}/UI/Lang/UI_Chinese.h
          --source ${CMAKE_CURRENT_SOURCE_DIR}/Apps/user/khicas/khicas_zh.h
          --source ${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/kdisplay.cc
          --write ${CJK_GEN_DIR}/cjk_font_data.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/UI/Lang/UI_Chinese.h
          ${CMAKE_CURRENT_SOURCE_DIR}/Apps/user/khicas/khicas_zh.h
          ${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/kdisplay.cc
          ${CMAKE_SOURCE_DIR}/Scripts/gen_cjk_font.py
  COMMENT "Generating CJK glyph table cjk_font_data.h from UI_Chinese.h (UTF-8)"
  VERBATIM
)
add_custom_target(gen_cjk DEPENDS ${CJK_GEN_DIR}/cjk_font_data.h)
add_dependencies(sys.elf gen_cjk)
# khicas_lib compiles khicas_cjk.cpp, which includes the generated table; make it
# wait for gen_cjk (the target is defined here, not in the khicas subdirectory).
add_dependencies(khicas_lib gen_cjk)
include_directories(${CJK_GEN_DIR})
# -----------------------------------------------------------------------------

configure_file(
  ${CMAKE_SOURCE_DIR}/Script/sys_ld.script.in
  ${CMAKE_CURRENT_BINARY_DIR}/sys_ld.script
  @ONLY
)

set(LINKER_SCRIPT 
${CMAKE_CURRENT_BINARY_DIR}/sys_ld.script
)
set(LINKER_FLAGS "SHELL:-T${LINKER_SCRIPT} -Wl,--wrap=malloc -Wl,--wrap=free")
target_link_options(sys.elf PRIVATE ${LINKER_FLAGS}) 

 
target_compile_options(tjpgd PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -mthumb-interwork -mthumb -Os)


target_compile_options(sys.elf PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -marm
-Os -pipe
-DHP39
-Wno-deprecated-declarations
$<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
$<$<COMPILE_LANGUAGE:CXX>:-Wno-template-id-cdtor>
$<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
# Our own C++ uses zero exception/RTTI features (verified: 0 throw/try/catch/typeid/
# dynamic_cast in first-party .cpp). Disable the machinery so our objects emit no
# landing pads or RTTI metadata. The prebuilt giac/GMP blobs keep their own unwinder;
# this only strips OUR overhead. -fno-use-cxa-atexit: init.c walks __ctors and runs no
# dtors (atexit.cpp is a no-op __cxa_atexit stub). -fno-threadsafe-statics: no
# function-local statics with non-trivial ctors exist.
$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-fno-use-cxa-atexit;-fno-threadsafe-statics>
)

# Build sys.elf's own C++ at C++23 (target-scoped so khicas_lib/tjpgd, which carry their
# own per-file -std, are untouched; the root default stays CMAKE_CXX_STANDARD 20).
set_property(TARGET sys.elf PROPERTY CXX_STANDARD 23)
set_property(TARGET sys.elf PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(sys.elf -lm -lc -lrdimon -lgcc -lstdc++ -nostartfiles
tjpgd
# The prebuilt KhiCAS/CAS blobs and khicas_lib are mutually dependent: the GUI blob
# calls porting stubs in khicas_lib, and khicas_repl.cpp (in khicas_lib) calls back
# into the blobs (xcas::Console_*, giac::*). Single-pass link order can't satisfy a
# cycle, so wrap them in a group the linker iterates until all refs resolve.
-Wl,--start-group
${CMAKE_SOURCE_DIR}/External/KhiCAS/libcas.libcpp
${CMAKE_SOURCE_DIR}/External/LibTomMath/libtommath.libcpp
${CMAKE_SOURCE_DIR}/External/GMP/libgmp.libcpp
khicas_lib
-Wl,--end-group
)

# build time in UTC ISO 8601
FILE (WRITE ${CMAKE_BINARY_DIR}/timestamp.cmake "STRING(TIMESTAMP TIMEZ UTC)\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(WRITE timestamp.h \"#ifndef TIMESTAMP_H\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND timestamp.h \"#define TIMESTAMP_H\\n\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND timestamp.h \"#define _TIMEZ_ \\\"\${TIMEZ}\\\"\\n\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND timestamp.h \"#endif // TIMESTAMP_H\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(WRITE UI_build_stamp.h \"#pragma once\\n\")\n")
FILE (APPEND ${CMAKE_BINARY_DIR}/timestamp.cmake "FILE(APPEND UI_build_stamp.h \"#define BUILD_STAMP \\\"Development build\\\"\\n\")\n")

ADD_CUSTOM_TARGET (
    timestamp
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/timestamp.cmake
    ADD_DEPENDENCIES ${CMAKE_BINARY_DIR}/timestamp.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(sys.elf timestamp)

add_custom_command(TARGET sys.elf  POST_BUILD
    COMMAND arm-none-eabi-size ${CMAKE_CURRENT_BINARY_DIR}/sys.elf 
    )

add_custom_target(
  ExistOS.sys ALL
  COMMAND ${CMAKE_OBJCOPY} -I elf32-little -O binary -R .note -R .comment -S ${CMAKE_CURRENT_BINARY_DIR}/sys.elf ${CMAKE_CURRENT_BINARY_DIR}/ExistOS.sys
  DEPENDS sys.elf
  )

  add_custom_target(
    edb_flash_sys 
    COMMAND ${CMAKE_SOURCE_DIR}/tools/edb -r -s -f ${CMAKE_CURRENT_BINARY_DIR}/ExistOS.sys 1984
    DEPENDS ExistOS.sys
    )

add_dependencies(edb_flash_sys timestamp)
