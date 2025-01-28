# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

cmake_minimum_required(VERSION 3.5)

#set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/components)

set(EXTRA_COMPONENT_DIRS  
    /home/lucia/esp/v5.3.1/esp-idf/examples/system/console/advanced/components

) 

include(/home/lucia/esp/v5.3.1/esp-idf/tools/cmake/project.cmake)
project(esp32s2-ftm-anchor)


############

