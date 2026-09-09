# Daisy STM32H750 Toolchain Configuration
# This file sets up cross-compilation for Daisy Seed 3

# Get DAISY_ROOT from environment
if(DEFINED ENV{DAISY_ROOT})
    set(DAISY_ROOT $ENV{DAISY_ROOT})
else()
    message(FATAL_ERROR "DAISY_ROOT not set")
endif()

# Find the ARM GNU toolchain
find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_NONE_EABI_CXX arm-none-eabi-g++ REQUIRED)
find_program(ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(ARM_NONE_EABI_SIZE arm-none-eabi-size REQUIRED)

# Get directory containing the toolchain
get_filename_component(ARM_TOOLCHAIN_PATH ${ARM_NONE_EABI_GCC} DIRECTORY)

# Set compilers
set(CMAKE_C_COMPILER ${ARM_NONE_EABI_GCC})
set(CMAKE_CXX_COMPILER ${ARM_NONE_EABI_CXX})
set(CMAKE_ASM_COMPILER ${ARM_NONE_EABI_GCC})
set(CMAKE_OBJCOPY ${ARM_NONE_EABI_OBJCOPY})
set(CMAKE_SIZE ${ARM_NONE_EABI_SIZE})

# Use ARM GNU Toolchain flags
include(${DAISY_ROOT}/cmake/toolchains/ArmGNUToolchain.cmake)

# Use STM32H750 specific configuration
include(${DAISY_ROOT}/cmake/toolchains/stm32h750xx.cmake)
