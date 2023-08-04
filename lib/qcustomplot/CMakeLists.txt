cmake_minimum_required()

project(QCustomPlot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CXX_EXTENSIONS YES)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QCP_SRCS qcustomplot.h qcustomplot.cpp)

find_package(QT NAMES Qt6 EQUIRED COMPONENTS Core Gui PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui PrintSupport)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_library(${PROJECT_NAME} MANUAL_FINALIZATION ${QCP_SRCS})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::PrintSupport)
