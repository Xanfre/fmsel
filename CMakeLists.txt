cmake_minimum_required(VERSION 3.12)
project(fmsel)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TOOLS "Build FMSel tools" ON)
option(MP3_SUPPORT "Enable MP3 support" ON)
option(OGG_SUPPORT "Enable Ogg Vorbis support" ON)
option(OPUS_SUPPORT "Enable Opus support" ON)
option(FLAC_SUPPORT "Enable FLAC support" ON)
option(T3_SUPPORT "Enable support for T3 FMs" OFF)
option(GLML_SUPPORT "Enable support for GLML readmes" ${T3_SUPPORT})
option(USE_SHARED_FLTK "Link with FLTK shared libraries" OFF)

find_package(FLTK 1.4 CONFIG REQUIRED)

set( CPM_DOWNLOAD_VERSION 0.43.1 )
set( CPM_DOWNLOAD_HASH "1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934" )
if( CPM_SOURCE_CACHE )
    set( CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake" )
elseif( DEFINED ENV{CPM_SOURCE_CACHE} )
    set( CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake" )
else()
    set( CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake" )
endif()
get_filename_component( CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE )
set( CPM_USE_NAMED_CACHE_DIRECTORIES ON )
if( NOT ( EXISTS ${CPM_DOWNLOAD_LOCATION} ) )
    message( STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}" )
    file( DOWNLOAD
          https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
          ${CPM_DOWNLOAD_LOCATION}
          SHOW_PROGRESS
          EXPECTED_HASH SHA256=${CPM_DOWNLOAD_HASH}
    )
endif()
include( ${CPM_DOWNLOAD_LOCATION} )

list(APPEND
	BIT7Z_OPTIONS
	"BIT7Z_AUTO_FORMAT ON"
	"BIT7Z_INSTALL OFF"
)
if (NOT WIN32)
	list(APPEND BIT7Z_OPTIONS "BIT7Z_GENERATE_PIC ON")
endif()
CPMAddPackage(
	NAME bit7z
	GIT_REPOSITORY "https://github.com/rikyoz/bit7z"
	VERSION "4.1.0"
	OPTIONS "${BIT7Z_OPTIONS}"
)
if (NOT WIN32)
	set_property(TARGET bit7z PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

find_package(PkgConfig REQUIRED)

include_directories(
	${fmsel_SOURCE_DIR}
)
if (OGG_SUPPORT)
	pkg_check_modules(OGG REQUIRED vorbis vorbisfile)
	include_directories("${OGG_INCLUDE_DIRS}")
endif()
if (OPUS_SUPPORT)
	pkg_check_modules(OPUS REQUIRED opus opusfile)
	include_directories("${OPUS_INCLUDE_DIRS}")
endif()
if (FLAC_SUPPORT)
	pkg_check_modules(FLAC REQUIRED flac)
	include_directories("${FLAC_INCLUDE_DIRS}")
endif()

if (MP3_SUPPORT OR OGG_SUPPORT OR OPUS_SUPPORT OR FLAC_SUPPORT)
	CPMAddPackage(
		NAME dr_libs
		GIT_REPOSITORY "https://github.com/mackron/dr_libs"
		GIT_TAG "fa931f3285ced10ace628f7f1ac951e1951e7ea6"
		GIT_SUBMODULES "tests/common"
		DOWNLOAD_ONLY TRUE
	)
	include_directories("${dr_libs_SOURCE_DIR}")

	add_compile_definitions(AUDIO_SUPPORT)
	if (MP3_SUPPORT)
		add_compile_definitions(MP3_SUPPORT)
	endif()
	if (OGG_SUPPORT)
		add_compile_definitions(OGG_SUPPORT)
	endif()
	if (OPUS_SUPPORT)
		add_compile_definitions(OPUS_SUPPORT)
	endif()
	if (FLAC_SUPPORT)
		add_compile_definitions(FLAC_SUPPORT)
	endif()
endif()
if (T3_SUPPORT)
	add_compile_definitions(T3_SUPPORT)
endif()
if (T3_SUPPORT OR GLML_SUPPORT)
	add_compile_definitions(GLML_SUPPORT)
endif()
if (NOT WIN32)
	add_compile_definitions(USE_PATH_MAX USE_XDG)
	add_compile_options("SHELL:-include unixcompat.h")
endif()

add_library(
	fmsel
	SHARED
	archive.cpp
	archive.h
	dbgutil.h
	fl_mainwnd.cpp.h
	fl_mainwnd.h
	fl_taged.cpp.h
	fl_taged.h
	flstring.h
	fmsel.cpp
	fmsel.h
	glml.cpp
	glml.h
	icons.h
	lang.cpp
	lang.h
	mp3.cpp
	mp3.h
	os.cpp
	os.h
	Fl_Html_View.cpp
	Fl_Table/Fl_Table.H
	Fl_Table/Fl_Table.cxx
	Fl_Table/Fl_Table_Row.H
	Fl_Table/Fl_Table_Row.cxx
	Fle_Colors.cpp
	Fle_Schemes.cpp
)

if (USE_SHARED_FLTK)
	target_link_libraries(fmsel PRIVATE fltk::fltk-shared fltk::images-shared bit7z)
else()
	target_link_libraries(fmsel PRIVATE fltk::fltk fltk::images bit7z)
endif()
if (OGG_SUPPORT)
	target_link_libraries(fmsel PRIVATE ${OGG_LIBRARIES})
endif()
if (OPUS_SUPPORT)
	target_link_libraries(fmsel PRIVATE ${OPUS_LIBRARIES})
endif()
if (FLAC_SUPPORT)
	target_link_libraries(fmsel PRIVATE ${FLAC_LIBRARIES})
endif()

if (WIN32)
	if (NOT MSVC)
		target_link_options(fmsel PRIVATE "-Wl,--exclude-all-symbols")
	endif()
else()
	target_link_options(fmsel PRIVATE "-Wl,--no-undefined")
endif()

set_target_properties(
	fmsel
	PROPERTIES
	SOVERSION 1
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON
)

install(
	TARGETS
	fmsel
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
	FILES
	fmsel.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if (BUILD_TOOLS)
	if (WIN32)
		set(EXE_TYPE WIN32)
	endif()

	add_executable(
		fmsel_launch
		${EXE_TYPE}
		fmsel_launch.c
	)

	target_link_libraries(fmsel_launch PRIVATE fmsel)

	install(
		TARGETS
		fmsel_launch
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
endif()
