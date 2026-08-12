# Collect test source files
file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/**/*.c"
)

# Add test executable
# If you want to test a new source file, you need to add it to the test executable
add_executable(${PROJECT_NAME}_unit_tests
    ${TEST_SOURCES}
    # Library code (lib/)
    ${CMAKE_SOURCE_DIR}/lib/input/button.c
    ${CMAKE_SOURCE_DIR}/lib/input/cv_input.c
    ${CMAKE_SOURCE_DIR}/lib/output/cv_output.c
    ${CMAKE_SOURCE_DIR}/lib/output/led_animation.c
    ${CMAKE_SOURCE_DIR}/lib/utility/delay.c
    ${CMAKE_SOURCE_DIR}/lib/fsm/fsm.c
    ${CMAKE_SOURCE_DIR}/lib/events/events.c
    # Application code (src/)
    ${CMAKE_SOURCE_DIR}/src/output/led_feedback.c
    ${CMAKE_SOURCE_DIR}/src/app_init.c
    ${CMAKE_SOURCE_DIR}/src/modes/mode_handlers.c
    ${CMAKE_SOURCE_DIR}/src/core/coordinator.c
)

# Add test include directories
target_include_directories(${PROJECT_NAME}_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/unity/src
    ${CMAKE_SOURCE_DIR}/external/unity/extras/fixture/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/test/unit
)

target_link_libraries(${PROJECT_NAME}_unit_tests PRIVATE
    unity::framework
)

# Register with CTest
add_test(
    NAME unit_tests
    COMMAND ${PROJECT_NAME}_unit_tests
)
