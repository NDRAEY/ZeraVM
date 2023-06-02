cmake_minimum_required(VERSION 3.18)
PROJECT("ZeraVM")

add_compile_options(-ggdb3 -Wall -I../include)

set(
	SRC
	src/main.cpp
)

add_executable(zera ${SRC})
