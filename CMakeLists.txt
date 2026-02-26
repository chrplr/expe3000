cmake_minimum_required(VERSION 3.16)
project(expe3000 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(SOURCES 
    src/main.c 
    src/csv_parser.c
    src/argparse.c
)

# Use PkgConfig to find SDL3 and its components
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)

# Include directories
include_directories(include ${SDL3_INCLUDE_DIRS} ${SDL3_IMAGE_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})

# Executable
add_executable(expe3000 ${SOURCES})

# Link libraries
target_link_libraries(expe3000 PRIVATE 
    ${SDL3_LIBRARIES} 
    ${SDL3_IMAGE_LIBRARIES} 
    ${SDL3_TTF_LIBRARIES}
)

# Add compiler options
target_compile_options(expe3000 PRIVATE -Wno-missing-field-initializers ${SDL3_CFLAGS_OTHER} ${SDL3_IMAGE_CFLAGS_OTHER} ${SDL3_TTF_CFLAGS_OTHER})
