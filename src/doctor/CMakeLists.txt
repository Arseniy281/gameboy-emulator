add_executable(gb_doctor main.cpp)
target_link_libraries(gb_doctor PRIVATE gb_cpu gb_ppu)
target_include_directories(gb_doctor PRIVATE ${CMAKE_SOURCE_DIR}/src/cpu)
