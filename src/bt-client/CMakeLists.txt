cmake_minimum_required(VERSION 3.12)
project(bt-client C CXX ASM)

file(GLOB FILES *.c *.h)

add_library(${PROJECT_NAME} ${FILES})

target_link_libraries(${PROJECT_NAME}
	FreeRTOS-Kernel-Heap4
	pico_stdlib
	pico_btstack_ble
	pico_btstack_classic
    pico_btstack_cyw43
	pico_cyw43_arch_none
	mpack
	commands
	BTSTACK_PORT
	FREERTOS_PORT
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
	ENABLE_CLASSIC=1
	RUNNING_AS_CLIENT=1
	CYW43_ENABLE_BLUETOOTH=1
)

target_include_directories(${PROJECT_NAME} PUBLIC
	include
)