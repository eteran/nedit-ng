
include("DetectCompiler")

function(TARGET_ADD_WARNINGS TARGET)
	if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
		target_compile_options(${TARGET}
		PUBLIC
			-W
			-Wall
			#-Wconversion
			#-Wnull-dereference
			#-Wold-style-cast
			#-Wshadow
			#-Wsign-conversion
			-Wcast-align
			-Wdouble-promotion
			-Wformat=2
			-Wno-switch-enum
			-Wno-unknown-pragmas
			-Wno-unused-macros
			-Wnon-virtual-dtor
			-Woverloaded-virtual
			-Wunused
			-pedantic
		)

		if(TARGET_COMPILER_CLANG)
			target_compile_options(${TARGET}
			PUBLIC
				#-Wexit-time-destructors
				#-Wglobal-constructors
				#-Wshadow-uncaptured-local
				#-Wshorten-64-to-32
				-Wconditional-uninitialized
				-Wimplicit-fallthrough
				-Wmissing-prototypes
			)

	    elseif(TARGET_COMPILER_GCC)
			target_compile_options(${TARGET}
			PUBLIC
			    -Wduplicated-branches
				-Wduplicated-cond
				#-Wuseless-cast
				-Wlogical-op
				-Wsuggest-final-methods
				-Wsuggest-final-types
				-Wsuggest-override
			)
	    endif()
	endif()
endfunction()
