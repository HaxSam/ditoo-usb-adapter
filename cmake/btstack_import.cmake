add_library(BTSTACK_PORT INTERFACE)
target_include_directories(BTSTACK_PORT
   INTERFACE
   ${CMAKE_SOURCE_DIR}/configs/btstack
)