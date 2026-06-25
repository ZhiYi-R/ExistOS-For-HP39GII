

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
#aux_source_directory(./filesystem/littlefs                          SYS_SRCS)
aux_source_directory(./Crash                                         SYS_SRCS)

# 添加getentropy.c文件到编译源文件
list(APPEND SYS_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/Core/getentropy.c)

#add_subdirectory(gb)
#add_subdirectory(KLib)
add_subdirectory(${CMAKE_SOURCE_DIR}/External/TJpgDec tjpgd)
#add_subdirectory(emu48)
add_subdirectory(Apps/user/khicas)

add_executable(sys.elf ${SYS_SRCS})

# --- Generate GBK copy of the UTF-8 UI_Chinese.h at build time ---------------
# The on-screen CJK renderer (draw_char_GBK16) decodes GB2312 and arm-none-eabi-gcc
# has no iconv, so string literals must already be GBK bytes in the source fed to the
# compiler. We keep the checked-in source UTF-8 (editor/git/CI friendly) and transcode
# it here into a differently-named header so it does not shadow the UTF-8 source via
# same-directory quote-include. UI_Language.h includes the generated UI_Chinese_gbk.h.
find_program(UV uv)
if(NOT UV)
  message(FATAL_ERROR "uv not found: it is required to transcode UI_Chinese.h (UTF-8) to GBK. Install uv or add it to PATH.")
endif()

set(LANG_GBK_DIR ${CMAKE_CURRENT_BINARY_DIR}/lang_gbk)
add_custom_command(
  OUTPUT ${LANG_GBK_DIR}/UI_Chinese_gbk.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${LANG_GBK_DIR}
  COMMAND ${UV} run -q --project ${CMAKE_SOURCE_DIR}/Scripts python
          ${CMAKE_SOURCE_DIR}/Scripts/transcode.py
          ${CMAKE_CURRENT_SOURCE_DIR}/UI/Lang/UI_Chinese.h
          ${LANG_GBK_DIR}/UI_Chinese_gbk.h
          gbk
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/UI/Lang/UI_Chinese.h
          ${CMAKE_SOURCE_DIR}/Scripts/transcode.py
  COMMENT "Transcoding UI_Chinese.h (UTF-8) -> UI_Chinese_gbk.h (GBK)"
  VERBATIM
)
add_custom_target(gen_lang_gbk DEPENDS ${LANG_GBK_DIR}/UI_Chinese_gbk.h)
add_dependencies(sys.elf gen_lang_gbk)
include_directories(${LANG_GBK_DIR})
# -----------------------------------------------------------------------------

set(FONT_HZK16S_LD "${CMAKE_SOURCE_DIR}/fonts/fonts_hzk16s.ld")
configure_file(
  ${CMAKE_SOURCE_DIR}/Script/sys_ld.script.in
  ${CMAKE_CURRENT_BINARY_DIR}/sys_ld.script
  @ONLY
)

set(LINKER_SCRIPT 
${CMAKE_CURRENT_BINARY_DIR}/sys_ld.script
)
set(LINKER_FLAGS "SHELL:-T${LINKER_SCRIPT} -Wl,--wrap=malloc -Wl,--wrap=free")
#-Wl,--gc-sections  
target_link_options(sys.elf PRIVATE ${LINKER_FLAGS}) 

 
#target_compile_options(gb PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -mthumb -Ofast)
#target_compile_options(emu48 PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -mthumb -Os)
#target_compile_options(lvgl PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -mthumb -Os)
target_compile_options(tjpgd PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -mthumb-interwork -mthumb -Os)


#-mthumb
target_compile_options(sys.elf PRIVATE -mtune=arm926ej-s -mcpu=arm926ej-s -mlittle-endian -mfloat-abi=soft -marm
-Os -pipe
-DHP39
# suppress KhiCAS headers C++20 deprecated warnings (C++ only)
-Wno-deprecated-declarations
$<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
$<$<COMPILE_LANGUAGE:CXX>:-Wno-template-id-cdtor>
$<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
#-ffunction-sections -fno-strict-aliasing -fomit-frame-pointer
#-fcommon -fno-strict-aliasing -fomit-frame-pointer
#-fpermissive  -fdata-sections
#-fno-exceptions
)

#set_property(TARGET lvgl PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#set_property(TARGET gb PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_link_libraries(sys.elf -lm -lc -lrdimon -lgcc -lstdc++ -nostartfiles
tjpgd
#gb 
#emu48
#lvgl
${CMAKE_SOURCE_DIR}/External/KhiCAS/libkcasgui.libcpp
#${CMAKE_SOURCE_DIR}/External/MicroPython/libmpy.libc
${CMAKE_SOURCE_DIR}/External/KhiCAS/libcas.libcpp
#${CMAKE_SOURCE_DIR}/External/KhiCAS/libgiac.libcpp
${CMAKE_SOURCE_DIR}/External/LibTomMath/libtommath.libcpp
${CMAKE_SOURCE_DIR}/External/GMP/libgmp.libcpp
khicas_lib
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
  COMMAND ${CMAKE_NM} -n ${CMAKE_CURRENT_BINARY_DIR}/sys.elf > ${CMAKE_CURRENT_BINARY_DIR}/sys_symtab.txt 
  COMMAND ${CMAKE_SOURCE_DIR}/tools/sys_signer ${CMAKE_CURRENT_BINARY_DIR}/sys_symtab.txt ${CMAKE_CURRENT_BINARY_DIR}/ExistOS.sys 
  DEPENDS sys.elf 
  )

  add_custom_target(
    edb_flash_sys 
    COMMAND ${CMAKE_SOURCE_DIR}/tools/edb -r -s -f ${CMAKE_CURRENT_BINARY_DIR}/ExistOS.sys 1984
    DEPENDS ExistOS.sys
    )

add_dependencies(edb_flash_sys timestamp)
