cmake_minimum_required(VERSION 3.25)

# set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# import the pico-sdk
set(ENV{PICO_SDK_FETCH_FROM_GIT} 1)
include(pico_sdk_import.cmake)

include(FetchContent)

FetchContent_Declare(libgps
    GIT_REPOSITORY https://github.com/n9wxu/libgps.git
)

FetchContent_MakeAvailable(libgps)
add_subdirectory(${libgps_SOURCE_DIR}/src)

# TODO fix this hard path to the FreeRTOS kernel
# Ideally make this a fetchcontent
set(ENV{FREERTOS_KERNEL_PATH} ~/Documents/working/FreeRTOS-kernel)
include($ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

message(VERSION_REVISION="${PICO_SDK_VERSION_REVISION}")

project(weather C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.c
    rain_task.c
    wind_task.c
    gps_task.c
    reporting_task.c
    temperature_task.c
    pressure_task.c)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/switch_inputs.pio)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    hardware_pio
    hardware_rtc
    hardware_uart
    hardware_gpio
    hardware_i2c
    libgps
)

pico_add_extra_outputs(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
