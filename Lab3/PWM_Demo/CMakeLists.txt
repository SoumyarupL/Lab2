add_executable(pwm_demo)

pico_generate_pio_header(pwm_demo ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(pwm_demo ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(pwm_demo ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)

# must match with executable name and source file names
target_sources(pwm_demo PRIVATE pwm-demo.c vga_graphics.c mpu6050.c)

# Add pico_multicore which is required for multicore functionality
target_link_libraries(pwm_demo pico_stdlib pico_multicore pico_bootsel_via_double_reset hardware_pwm hardware_dma hardware_irq hardware_adc hardware_pio hardware_i2c)

# create map/bin/hex file etc.
pico_add_extra_outputs(pwm_demo)