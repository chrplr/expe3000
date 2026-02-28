cmake_minimum_required(VERSION 3.16)
project(expe3000 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(SOURCES 
    src/main.c 
    src/csv_parser.c
    src/argparse.c
    src/dlp.c
)

# Use PkgConfig to find SDL3 and its components
find_package(PkgConfig REQUIRED)

# We use IMPORTED_TARGET so that PkgConfig::... targets are created.
# These targets automatically include the correct -L library paths.
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(SDL3_IMAGE REQUIRED IMPORTED_TARGET sdl3-image)
pkg_check_modules(SDL3_TTF REQUIRED IMPORTED_TARGET sdl3-ttf)

# Include directories (include/ is local, the others are handled by the targets)
include_directories(include)

# Executable
add_executable(expe3000 ${SOURCES})

# Link libraries against the imported targets
target_link_libraries(expe3000 PRIVATE 
    PkgConfig::SDL3 
    PkgConfig::SDL3_IMAGE 
    PkgConfig::SDL3_TTF
)

# Add compiler options
target_compile_options(expe3000 PRIVATE -Wno-missing-field-initializers)

# Ensure console output works on Windows (prevents stdout from being suppressed)
if(WIN32)
    if(MSVC)
        target_link_options(expe3000 PRIVATE /SUBSYSTEM:CONSOLE)
    else()
        target_link_options(expe3000 PRIVATE -mconsole)
    endif()
endif()
