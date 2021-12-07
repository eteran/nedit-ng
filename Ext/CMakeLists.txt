cmake_minimum_required(VERSION 3.0)

add_library(Ext INTERFACE)

target_sources(Ext INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}/include/Ext/variant.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/Ext/optional.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/Ext/string_view.h)


target_include_directories(Ext
INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/include
)

set_property(TARGET Ext PROPERTY CXX_STANDARD ${TARGET_COMPILER_HIGHEST_STD_SUPPORTED})
set_property(TARGET Ext PROPERTY CXX_EXTENSIONS OFF)
