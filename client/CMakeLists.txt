cmake_minimum_required(VERSION 3.15)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "../src/res/nedit-ng.rc")
endif()

set(PROJECT_SOURCES
	nc.cpp
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(nc-ng
		MANUAL_FINALIZATION
		${PROJECT_SOURCES}
		${APP_ICON_RESOURCE_WINDOWS}
    )
else()
    add_executable(nc-ng
		${PROJECT_SOURCES}
		${APP_ICON_RESOURCE_WINDOWS}
    )
endif()

target_add_warnings(nc-ng)

target_link_libraries(nc-ng
PRIVATE
	Util
	Settings
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Network
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
	set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.nc-ng)
endif()

set_target_properties(nc-ng PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(TARGET_COMPILER_MSVC)
	target_compile_definitions(nc-ng
		PUBLIC -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	)
endif()

include(GNUInstallDirs)
install(TARGETS nc-ng
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(nc-ng)
endif()

