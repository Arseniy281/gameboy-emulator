add_library(gb_cpu
    CPU.cpp
)

target_include_directories(gb_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(gb_cpu PUBLIC GB_CPU_LOGGING=0)
