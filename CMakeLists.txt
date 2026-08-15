cmake_minimum_required(VERSION 3.16)

project(BusValidator
    VERSION 0.1.0
    DESCRIPTION "Open-loop fare collection terminal for public transport"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

# Reads the hex dump left by the expect script, extracts PAN and expiry,
# calls the payment backend and reports the outcome to the kiosk UI.
add_executable(parse_card_data src/reader/parse_card_data.cpp)
target_link_libraries(parse_card_data PRIVATE Threads::Threads)

# Bench tool: pushes a synthetic card record into the FIFO so the UI can be
# exercised without a reader attached.
add_executable(fifo_writer src/tools/fifo_writer.cpp)

install(TARGETS parse_card_data fifo_writer RUNTIME DESTINATION bin)
