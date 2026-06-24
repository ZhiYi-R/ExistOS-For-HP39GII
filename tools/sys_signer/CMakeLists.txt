cmake_minimum_required(VERSION 3.16)


project(sys_signer VERSION 0.1.0)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
SET(BUILD_SHARED_LIBRARIES OFF)


include_directories(.)
include_directories(./libelf)

aux_source_directory(.              EXE_SRCS)

add_subdirectory(libelf)

add_executable(sys_signer ${EXE_SRCS})
 
SET_TARGET_PROPERTIES(sys_signer PROPERTIES COMPILE_FLAGS "-O0 -static" ) 


target_link_libraries(sys_signer -lm -lgcc -lstdc++
libelf -static
)

