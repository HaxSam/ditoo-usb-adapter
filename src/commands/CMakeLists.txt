cmake_minimum_required(VERSION 3.12)
project(commands C CXX ASM)

add_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	FreeRTOS-Kernel-Heap4
	FREERTOS_PORT
)

target_include_directories(${PROJECT_NAME} INTERFACE
	.
)