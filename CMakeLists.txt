cmake_minimum_required(VERSION 3.22)
project(ByteOS LANGUAGES C ASM)

# -----------------------------------------------------------
# Global settings
# -----------------------------------------------------------
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_ASM_COMPILER gcc)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# -----------------------------------------------------------
# Source files
# -----------------------------------------------------------
file(GLOB_RECURSE CORE_SRCS
    src/*/*.c
    src/drivers/**/*.c
)

file(GLOB_RECURSE ASM_SRCS
    src/*.S
)

# -----------------------------------------------------------
# Kernel executable
# -----------------------------------------------------------
add_executable(kernel.elf ${CORE_SRCS} ${ASM_SRCS})

# Set target-specific compiler flags
target_compile_options(kernel.elf PRIVATE
    -m64
    -mcmodel=kernel
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fno-pie
    -O2
    -Wall -Wextra
)

# For assembly files
set_source_files_properties(${ASM_SRCS} PROPERTIES
    COMPILE_FLAGS "-m64 -mcmodel=kernel -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O2 -Wall -Wextra"
)

# Linker flags
set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-nostdlib -static -no-pie -T ${CMAKE_SOURCE_DIR}/linker.ld -z max-page-size=0x1000 -mcmodel=kernel"
)