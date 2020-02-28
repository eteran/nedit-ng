include("DetectCompiler")

if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
	option(ENABLE_PROFILING "Enable profiling")
	
	if(ENABLE_PROFILING)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -pg -no-pie")
		set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS}          -pg -no-pie")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
	endif()
endif()

