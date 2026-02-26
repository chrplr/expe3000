cmake_minimum_required(VERSION 3.16)
project(expe3000 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL3, SDL3_image, and SDL3_ttf
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)

# Include directories
include_directories(include)

# Source files
set(SOURCES 
    src/main.c 
    src/csv_parser.c
)

# Executable
add_executable(expe3000 ${SOURCES})

# Link libraries
target_link_libraries(expe3000 PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)

# Add compiler options
target_compile_options(expe3000 PRIVATE -Wno-missing-field-initializers)

# Installation/Runtime: Copy assets on build if needed (optional)
# add_custom_command(TARGET expe3000 POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:expe3000>/assets)
