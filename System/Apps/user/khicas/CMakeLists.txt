# KhiCAS application CMakeLists.txt

# 添加源文件
aux_source_directory(. KHICAS_SRCS)

# 创建静态库
add_library(khicas_lib STATIC ${KHICAS_SRCS})

# 设置库属性
target_include_directories(khicas_lib PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/
)

# khicas_repl.cpp pulls in the giac headers directly, so it must be built with the
# same defines the prebuilt giac/xcas libraries were compiled with (-DKHICAS
# -DHAVE_CONFIG_H) and needs libtommath on the include path. Applied per-file so the
# plain-C stub sources in this directory are unaffected. Keyed on the absolute path
# because aux_source_directory() lists sources as "./name" -- a bare "khicas_repl.cpp"
# would not match the same source object and the property would be silently ignored.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/khicas_repl.cpp PROPERTIES
    COMPILE_DEFINITIONS "KHICAS;HAVE_CONFIG_H")
target_include_directories(khicas_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/External/LibTomMath/libtommath-0.39)

# --- KhiCAS GUI compiled from source (spike, 阶段 1) ------------------------
# The prebuilt libkcasgui.libcpp held exactly three objects (kdisplay.o, kadd.o,
# file.o). To make the menus/help translatable in place we compile those same
# sources here instead and drop the blob from the link (System/CMakeLists.txt).
# Build them with the flags the blob was produced with (see the khicas Makefile):
# C++11, -fpermissive, no RTTI/exceptions. Under -DHP39 the Nspire/Numworks
# platform headers (libndls.h, os.h, dirent.h...) are #ifdef'd out, and config.h
# maps `ustl` -> `std`, so they need no extra SDK. The engine blobs
# (libcas/libgiac.libcpp) stay prebuilt; the GUI calls into them as before.
set(KHICAS_GUI_SRCS
    ${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/kdisplay.cc
    ${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/kadd.cc
    ${CMAKE_SOURCE_DIR}/External/KhiCAS/khicas/file.cc
)
target_sources(khicas_lib PRIVATE ${KHICAS_GUI_SRCS})
set_source_files_properties(${KHICAS_GUI_SRCS} PROPERTIES
    COMPILE_DEFINITIONS "KHICAS;HAVE_CONFIG_H"
    COMPILE_OPTIONS "-std=gnu++11;-fpermissive;-fno-rtti;-fno-exceptions;-fno-use-cxa-atexit;-fno-strict-aliasing;-Wno-narrowing")
# ---------------------------------------------------------------------------

# khicas_cjk.cpp bridges the C porting layer to the system's compile-time CJK
# glyph table (System/UI/Core/cjk_font.h), which #includes the generated
# cjk_font_data.h emitted by the gen_cjk target into System/cjk_gen. Both paths
# must be visible here; the gen_cjk dependency is wired in System/CMakeLists.txt
# (the target is defined there, after this subdirectory is added).
target_include_directories(khicas_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/System/UI/Core
    ${CMAKE_BINARY_DIR}/System/cjk_gen)

# 编译选项
target_compile_options(khicas_lib PRIVATE
    -mtune=arm926ej-s
    -mcpu=arm926ej-s
    -mlittle-endian
    -mfloat-abi=soft
    -marm
    -Os
    -pipe
    -DHP39
)

# 屏蔽 KhiCAS 头文件引发的废弃 API 警告（C++20 下旧代码常见）
target_compile_options(khicas_lib PRIVATE
    -Wno-deprecated-declarations
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-template-id-cdtor>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
)