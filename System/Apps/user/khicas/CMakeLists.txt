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