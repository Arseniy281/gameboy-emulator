add_library(gb_ppu
    PPU.cpp
)

target_include_directories(gb_ppu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/cpu)

target_link_libraries(gb_ppu PUBLIC gb_cpu)
