
include("DetectCompiler")

function(TARGET_ADD_WARNINGS TARGET)
	if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
		target_compile_options(${TARGET}
		PUBLIC
			-W
			-Wall
			#-Wshadow
			-Wnon-virtual-dtor
			#-Wold-style-cast
			-Wcast-align
			-Wunused
			-Woverloaded-virtual
			-pedantic
			#-Wconversion
			#-Wsign-conversion
			#-Wnull-dereference
			-Wdouble-promotion
			-Wformat=2
			-Wno-unused-macros
			-Wno-switch-enum
			-Wno-unknown-pragmas
			)

		if(TARGET_COMPILER_CLANG)
			target_compile_options(${TARGET}
			PUBLIC
				#-Wshorten-64-to-32
				-Wconditional-uninitialized
				#-Wshadow-uncaptured-local
				-Wmissing-prototypes
				#-Wexit-time-destructors
				#-Wglobal-constructors
				-Wimplicit-fallthrough
			)

	    elseif(TARGET_COMPILER_GCC)
			target_compile_options(${TARGET}
			PUBLIC
				#-Wuseless-cast
				-Wduplicated-cond
				-Wduplicated-branches
				-Wlogical-op

				#-Wsuggest-attribute=pure
				#-Wsuggest-attribute=const
				#-Wsuggest-attribute=noreturn
				-Wsuggest-final-types
				-Wsuggest-final-methods
				-Wsuggest-override
			)
	    endif()
	endif()
endfunction()
