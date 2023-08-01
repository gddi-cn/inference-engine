cmake_minimum_required(VERSION 3.14.0)
project(cv_samples)

file(GLOB test_src_files "*.c??")
# message(STATUS ${CMAKE_CURRENT_SOURCE_DIR})
foreach (file IN LISTS test_src_files)
    add_program(${file})
endforeach (file)