cmake_minimum_required(VERSION 2.6)
project(scsitest)

#add_definitions(-D_FILE_OFFSET_BITS=64)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

set(SOURCE_FILES scsi_test.c)

add_executable(scsitest ${SOURCE_FILES})

target_link_libraries(scsitest)
