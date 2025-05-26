cmake_minimum_required(VERSION 3.12)
project(usb-dev C CXX ASM)

file(GLOB FILES *.c *.h)

add_library(${PROJECT_NAME} ${FILES})

target_link_libraries(${PROJECT_NAME}
	FreeRTOS-Kernel-Heap4
	pico_stdlib
	pico_cyw43_arch_none
 	pico_unique_id
	tinyusb_device
	tinyusb_board
	mpack
	commands
	FREERTOS_PORT
)

target_include_directories(${PROJECT_NAME} PUBLIC
	include
)