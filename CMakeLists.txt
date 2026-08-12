cmake_minimum_required(VERSION 3.21)

# Build options
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_SIM "Build x86 simulator" OFF)
option(SIZE_REPORT "Run size analysis after build (requires bc)" ON)

# Set compilers BEFORE project() command
if(BUILD_TESTS OR BUILD_SIM)
    # Test/Sim build: use host GCC, no AVR toolchain needed
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_ASM_COMPILER "/usr/bin/gcc")
    if(BUILD_TESTS)
        add_compile_definitions(TEST_BUILD)
    endif()
    if(BUILD_SIM)
        add_compile_definitions(SIM_BUILD)
    endif()
else()
    # Firmware build: require AVR toolchain
    find_program(AVR_CC avr-gcc REQUIRED)
    find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
    find_program(AVR_SIZE_TOOL avr-size REQUIRED)
    find_program(AVR_STRIP avr-strip REQUIRED)

    # Optional: avr-objdump (for disassembly)
    find_program(AVR_OBJDUMP avr-objdump)

    # Optional: avrdude (only needed for flashing)
    find_program(AVRDUDE avrdude)
    if(NOT AVRDUDE)
        message(STATUS "avrdude not found - flash/fuse targets will not be available")
    endif()

    # Optional: size report dependencies
    find_program(BC_TOOL bc)
    if(SIZE_REPORT AND NOT BC_TOOL)
        message(WARNING "bc not found - size report disabled. Install with: sudo apt install bc")
        message(WARNING "Or build with: cmake -DSIZE_REPORT=OFF ..")
        set(SIZE_REPORT OFF)
    endif()

    set(CMAKE_C_COMPILER ${AVR_CC})
    set(CMAKE_ASM_COMPILER ${AVR_CC})
endif()

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project configuration
project(gatekeeper
    VERSION 0.8.0
    DESCRIPTION "Eurorack gate/trigger processor"
    LANGUAGES C
)

# Display name for documentation (capitalized brand name)
set(PROJECT_DISPLAY_NAME "Gatekeeper")

# Include directories
include_directories(src)
include_directories(lib)

if(BUILD_TESTS)
    # Test build configuration
    message(STATUS "Building tests")

    # Enable new CMake policies
    # set CMP0077 to NEW to prevent the UNITY_EXTENSION_FIXTURE from being set to OFF
    # by an option command in the Unity CMakeLists.txt
    cmake_policy(SET CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    add_compile_options(-Wall -Os)

    # Add Unity test framework
    set(UNITY_EXTENSION_FIXTURE ON)
    add_subdirectory(external/Unity)

    # Enable CTest
    enable_testing()
    add_subdirectory(test/unit)
elseif(BUILD_SIM)
    # Simulator build configuration
    message(STATUS "Building x86 simulator")
    add_subdirectory(sim)
else()
    message(STATUS "Building application")

    # AVR settings
    set(MCU attiny85)
    set(F_CPU 8000000UL)

    # Programmer settings (can be overridden via cmake -DPROGRAMMER=... -DPROGRAMMER_PORT=...)
    set(PROGRAMMER "stk500v2" CACHE STRING "AVR programmer type (e.g., stk500v2, usbasp, avrisp2)")
    set(PROGRAMMER_PORT "/dev/ttyACM1" CACHE STRING "Programmer serial port (e.g., /dev/ttyACM0, /dev/ttyUSB0)")

    set(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -Os -Wall -Wextra -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -fshort-enums -flto")
    set(CMAKE_EXE_LINKER_FLAGS "-mmcu=${MCU} -Wl,--gc-sections -flto")

    # Collect source files recursively
    file(GLOB_RECURSE SOURCES
        "src/*.c"
        "src/**/*.c"
        "lib/*.c"
        "lib/**/*.c"
    )

    # Source files
    add_executable(${PROJECT_NAME} ${SOURCES})

    # Generate hex file
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${AVR_STRIP} --strip-debug ${PROJECT_NAME}
        COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom ${PROJECT_NAME} ${PROJECT_NAME}.hex
    )

    # Optional: run size analysis
    if(SIZE_REPORT)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/size_report.sh ${PROJECT_NAME} CMakeFiles/${PROJECT_NAME}.dir/src
        )
    else()
        # Minimal size output when full report is disabled
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${AVR_SIZE_TOOL} --format=avr --mcu=${MCU} ${PROJECT_NAME}
        )
    endif()

    # Flash/fuse targets (only if avrdude is available)
    if(AVRDUDE)
        add_custom_target(flash
            COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PROGRAMMER_PORT} -b 115200
                    -U flash:w:${PROJECT_NAME}.hex:i
            DEPENDS ${PROJECT_NAME}
        )

        add_custom_target(fuses
            COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PROGRAMMER_PORT} -b 115200
                    -U lfuse:w:0xE2:m -U hfuse:w:0xDF:m
        )

        add_custom_target(read_fuses
            COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PROGRAMMER_PORT} -b 115200
                    -U lfuse:r:-:h -U hfuse:r:-:h
        )
    endif()
endif()
