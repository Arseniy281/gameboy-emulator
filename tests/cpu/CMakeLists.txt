add_executable(cpu_tests cpu_tests.cpp)
target_link_libraries(cpu_tests PRIVATE gb_cpu)

add_test(NAME cpu_placeholder COMMAND cpu_tests)

# Provide source dir to tests for locating ROM paths reliably
target_compile_definitions(cpu_tests PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# Disable CPU logging by default for performance
target_compile_definitions(cpu_tests PRIVATE GB_CPU_LOGGING=0)

# Register Blargg cpu_instrs individual tests, each with ROM path as argument
add_test(NAME cpu_instrs_01_special COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/01-special.gb")
add_test(NAME cpu_instrs_02_interrupts COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/02-interrupts.gb")
add_test(NAME cpu_instrs_03_op_sp_hl COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/03-op sp,hl.gb")
add_test(NAME cpu_instrs_04_op_r_imm COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/04-op r,imm.gb")
add_test(NAME cpu_instrs_05_op_rp COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/05-op rp.gb")
add_test(NAME cpu_instrs_06_ld_r_r COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/06-ld r,r.gb")
add_test(NAME cpu_instrs_07_jr_jp_call_ret_rst COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb")
add_test(NAME cpu_instrs_08_misc_instrs COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/08-misc instrs.gb")
add_test(NAME cpu_instrs_09_op_r_r COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/09-op r,r.gb")
add_test(NAME cpu_instrs_10_bit_ops COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/10-bit ops.gb")
add_test(NAME cpu_instrs_11_op_a_hl COMMAND cpu_tests "${CMAKE_SOURCE_DIR}/tests/roms/cpu_instrs/individual/11-op a,(hl).gb")

# Unit tests for core LD behavior
add_executable(cpu_unit_tests unit_tests.cpp)
target_link_libraries(cpu_unit_tests PRIVATE gb_cpu)

add_executable(ppu_unit_ly tests_ppu_ly.cpp)
target_link_libraries(ppu_unit_ly PRIVATE gb_cpu gb_ppu)
add_test(NAME ppu_unit_ly_increment COMMAND ppu_unit_ly)
add_test(NAME cpu_unit_ld_hl_r COMMAND cpu_unit_tests)

# Increase timeout for CPU instruction tests to 30 seconds to allow long-running ROMs
set_tests_properties(
  cpu_instrs_01_special cpu_instrs_02_interrupts cpu_instrs_03_op_sp_hl
  cpu_instrs_04_op_r_imm cpu_instrs_05_op_rp cpu_instrs_06_ld_r_r
  cpu_instrs_07_jr_jp_call_ret_rst cpu_instrs_08_misc_instrs cpu_instrs_09_op_r_r
  cpu_instrs_10_bit_ops cpu_instrs_11_op_a_hl
  PROPERTIES TIMEOUT 30
)
