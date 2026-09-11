cmake_minimum_required(VERSION 3.20)

# Get the absolute path to DAISY_ROOT (either from environment or error)
if(DEFINED ENV{DAISY_ROOT})
    set(DAISY_ROOT $ENV{DAISY_ROOT})
else()
    message(FATAL_ERROR "DAISY_ROOT environment variable not set. Please run: export DAISY_ROOT=~/daisy-dev/libDaisy")
endif()

# Verify DAISY_ROOT exists
if(NOT EXISTS "${DAISY_ROOT}")
    message(FATAL_ERROR "DAISY_ROOT directory does not exist: ${DAISY_ROOT}")
endif()

# Verify we have the necessary Daisy files
if(NOT EXISTS "${DAISY_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "libDaisy CMakeLists.txt not found at ${DAISY_ROOT}/CMakeLists.txt")
endif()

# Set the toolchain BEFORE calling project()
# Use the custom Daisy Seed 3 toolchain which includes ARM GNU Toolchain and STM32H750 config
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/DaisySeed3.cmake" CACHE FILEPATH "")
endif()

# Now declare our project
project(DCO_ONE_Daisy C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The custom Daisy toolchain blanks out CMake's default per-config optimization
# flags, so they must be set explicitly. The 128K internal flash region is
# tight, so optimize for size.
add_compile_options(-Os -ffunction-sections -fdata-sections)

# Add libDaisy as a subdirectory
add_subdirectory(${DAISY_ROOT} libDaisy EXCLUDE_FROM_ALL)

# Find DaisySP
set(DAISYSP_DIR "${DAISY_ROOT}/../DaisySP")
if(NOT EXISTS "${DAISYSP_DIR}")
    message(FATAL_ERROR "DaisySP not found at ${DAISYSP_DIR}")
endif()

# Add DaisySP as a subdirectory  
add_subdirectory(${DAISYSP_DIR} DaisySP EXCLUDE_FROM_ALL)

# Add source files for our application
set(SOURCES
    src/main.cpp
    src/oscillator.cpp
    src/filter.cpp
    src/lfo.cpp
    src/fm.cpp
    src/input_manager.cpp
    src/midi.cpp
    src/patch_storage.cpp
)

# Create executable
add_executable(dco_one_phase1 ${SOURCES})

# Link libraries
target_link_libraries(dco_one_phase1
    PUBLIC
        daisy
        DaisySP
)

# newlib-nano (--specs=nano.specs) strips float support from printf/snprintf;
# pull it back in since we format frequency/sample values with %f.
target_link_options(dco_one_phase1 PUBLIC -u _printf_float)


# Include directories
target_include_directories(dco_one_phase1
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${DAISYSP_DIR}
)

# STM32H750 memory map linker script (missing this leaves the binary linked
# at the GNU ld default address ~0x8000 instead of internal flash 0x08000000,
# which boots to garbage with no visible symptoms: no LED, no USB).
target_link_options(dco_one_phase1
    PRIVATE
        -T${DAISY_ROOT}/core/STM32H750IB_flash.lds
        LINKER:--gc-sections
        LINKER:--check-sections
        LINKER:--unresolved-symbols=report-all
)
set_target_properties(dco_one_phase1 PROPERTIES
    LINK_DEPENDS ${DAISY_ROOT}/core/STM32H750IB_flash.lds
)
