cmake_minimum_required(VERSION 3.15)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Xml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Xml)

find_package(X11 REQUIRED)

if(${X11_FOUND})
    if(TARGET_PLATFORM_MACOS)
        link_directories(/usr/X11/lib)
        link_directories(/opt/X11/lib)
    endif()
    if(TARGET_PLATFORM_OPENBSD)
        link_directories(/usr/X11R6/lib)
    endif()
endif()

if(WIN32)
	set(APP_ICON_RESOURCE_WINDOWS "../src/res/nedit-ng.rc")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(nedit-import
		MANUAL_FINALIZATION
		import.cpp
		${APP_ICON_RESOURCE_WINDOWS}
    )
else()
    add_executable(nedit-import
		import.cpp
		${APP_ICON_RESOURCE_WINDOWS}
    )
endif()

target_add_warnings(nedit-import)

target_link_libraries(nedit-import PUBLIC
	Settings
	Interpreter
	X11
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Xml
)

target_include_directories(nedit-import PRIVATE
	${X11_INCLUDE_DIR}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
	set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.nedit-import)
endif()

set_target_properties(nedit-import PROPERTIES
	${BUNDLE_ID_OPTION}
	MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
	MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
	MACOSX_BUNDLE TRUE
	WIN32_EXECUTABLE TRUE
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(TARGET_COMPILER_MSVC)
	target_compile_definitions(nedit-import
		PUBLIC -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	)
endif()

include(GNUInstallDirs)
install(TARGETS nedit-import
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(nedit-import)
endif()
