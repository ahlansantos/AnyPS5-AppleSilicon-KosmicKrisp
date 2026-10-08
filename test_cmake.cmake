cmake_minimum_required(VERSION 3.10)
project(test)
add_library(foo SHARED test7.c)
add_custom_command(TARGET foo PRE_LINK COMMAND echo "$<TARGET_OBJECTS:foo>")
