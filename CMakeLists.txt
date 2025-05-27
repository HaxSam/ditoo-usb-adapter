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
set(MPACK_PATH ${CMAKE_SOURCE_DIR}/lib/mpack)

set(CMAKE_BUILD_TYPE Debug)
set(PICO_PLATFORM "rp2040")
set(PICO_BOARD pico_w)

include(cmake/pico_sdk_import.cmake)
pico_sdk_init()

include(cmake/btstack_import.cmake)
include(cmake/lwip_import.cmake)
include(cmake/FreeRTOS_Kernel_import.cmake)
include(cmake/mpack.cmake)

project(ditoo-usb-adapter C CXX ASM)

file(GLOB FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src/usb-dev)
add_subdirectory(src/bt-client)
add_subdirectory(src/commands)

add_executable(${PROJECT_NAME} ${FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC
    FreeRTOS-Kernel-Heap4
	pico_stdlib
	pico_cyw43_arch_none
	usb-dev
	bt-client
	commands
	FREERTOS_PORT
)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)