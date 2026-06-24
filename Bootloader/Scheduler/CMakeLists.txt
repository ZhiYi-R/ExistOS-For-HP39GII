include_directories(${CMAKE_SOURCE_DIR}/External/FreeRTOS/Bootloader)
include_directories(${CMAKE_SOURCE_DIR}/External/FreeRTOS/Bootloader/include)
AUX_SOURCE_DIRECTORY(${CMAKE_SOURCE_DIR}/External/FreeRTOS/Bootloader DIR_KERNEL_SRCS)
AUX_SOURCE_DIRECTORY(${CMAKE_SOURCE_DIR}/External/FreeRTOS/Bootloader/porting DIR_KERNEL_SRCS)

ADD_LIBRARY(kernel ${DIR_KERNEL_SRCS})



