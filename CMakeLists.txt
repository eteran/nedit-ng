cmake_minimum_required (VERSION 3.0)

project (nedit CXX)

if(ENABLE_ASAN)
        set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -fsanitize=address")
        set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

if(ENABLE_LTO)
        set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -flto")
        set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -flto")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()



add_definitions(-DQT_NO_CAST_FROM_ASCII)
add_definitions(-DQT_NO_CAST_TO_ASCII)
#add_definitions(-DREPLACE_SCOPE)

add_subdirectory (util)
add_subdirectory (source)
add_subdirectory (client)

