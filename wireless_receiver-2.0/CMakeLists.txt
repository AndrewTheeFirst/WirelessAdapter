# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "C:/Espressif/custom_components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wireless_receiver-2.0)
