file(GLOB FILES ${MPACK_PATH}/src/mpack/*.c)

project(mpack C CXX ASM)

add_library(${PROJECT_NAME} ${FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
	${MPACK_PATH}/src
)
target_compile_definitions(${PROJECT_NAME} PUBLIC 
	MPACK_EXTENSIONS=1
)