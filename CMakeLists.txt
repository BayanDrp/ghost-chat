cmake_minimum_required(VERSION 3.16)
project(ghostchat CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra)

file(GLOB_RECURSE GC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM GC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(ghostchat_lib STATIC ${GC_SOURCES})
target_include_directories(ghostchat_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

option(GHOSTCHAT_BUILD_TESTS "Build ghostchat tests" ON)
if(GHOSTCHAT_BUILD_TESTS)
    enable_testing()
    file(GLOB_RECURSE GC_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    foreach(test_src ${GC_TESTS})
        file(SIZE ${test_src} test_size)
        if(test_size GREATER 0)
            get_filename_component(test_name ${test_src} NAME_WE)
            add_executable(${test_name} ${test_src})
            target_link_libraries(${test_name} PRIVATE ghostchat_lib)
            add_test(NAME ${test_name} COMMAND ${test_name})
        endif()
    endforeach()
endif()

file(GLOB_RECURSE GC_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/samples/*.cpp")
foreach(sample_src ${GC_SAMPLES})
    get_filename_component(sample_name ${sample_src} NAME_WE)
    add_executable(sample_${sample_name} ${sample_src})
    target_link_libraries(sample_${sample_name} PRIVATE ghostchat_lib)
endforeach()

add_executable(ghostchat src/main.cpp)
target_link_libraries(ghostchat PRIVATE ghostchat_lib)
