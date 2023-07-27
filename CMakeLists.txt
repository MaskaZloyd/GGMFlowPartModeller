cmake_minimum_required(VERSION 3.23)

project(GGMFlowPartModeller LANGUAGES CXX)

#---------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CXX_EXTENSIONS YES)

#--------------- 
set(MAIN_SRCS src/main.cpp)

#--------------- 
add_executable(${PROJECT_NAME} ${MAIN_SRCS})


