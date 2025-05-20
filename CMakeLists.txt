cmake_minimum_required(VERSION 3.12)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(FREERTOS_KERNEL_PATH ${CMAKE_SOURCE_DIR}/lib/FreeRTOS-Kernel)
set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/lib/pico-sdk)

include(btstack_import.cmake)
include(lwip_import.cmake) # lwip config files
SET(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/configs/FreeRTOS-Kernel" CACHE STRING "Local Config")
include_directories("${FREERTOS_CONFIG_FILE_DIRECTORY}") # FreeRTOS config files

set(CMAKE_BUILD_TYPE Debug)
set(PICO_PLATFORM "rp2040")
set(PICO_BOARD pico_w)

include(pico_sdk_import.cmake)
pico_sdk_init()

include(FreeRTOS_Kernel_import.cmake)

project(ditoo-usb-adapter C CXX ASM)
set(NAME PICO_FREERTOS)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB FILES src/*.c)

add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME}
	pico_async_context_freertos
    FreeRTOS-Kernel-Heap4
	pico_stdlib
	pico_btstack_ble
    pico_btstack_cyw43
	pico_cyw43_arch_none
	hardware_resets
	hardware_irq
	BTSTACK_PORT
	LWIP_PORT
	FREERTOS_PORT
)

target_include_directories(${PROJECT_NAME} PRIVATE
	src/include
	${CMAKE_CURRENT_LIST_DIR}
)


pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)