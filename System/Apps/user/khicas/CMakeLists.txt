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