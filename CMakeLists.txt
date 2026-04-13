cmake_minimum_required(VERSION 3.20)
project(JvmContext)

set(CMAKE_CXX_STANDARD 17)

find_package(JNI REQUIRED)

add_library(JvmContext SHARED library.cpp)

target_include_directories(JvmContext PRIVATE ${JNI_INCLUDE_DIRS})

if(WIN32)
    if(MSVC)
        set_target_properties(JvmContext PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
    elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_options(JvmContext PRIVATE "-static" "-static-libgcc" "-static-libstdc++")
    endif()
endif()