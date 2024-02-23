cmake_minimum_required(VERSION 3.4)

project(APCpp)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES Release Debug RelWithDebInfo CACHE STRING INTERNAL FORCE)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(APCpp SHARED Archipelago.cpp Archipelago.h)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)

# Point to the dependencies
if (WIN32)
	set(ALLEG_DEPS_INCLUDE "${CMAKE_BINARY_DIR}/allegro5/deps/include/")
	set(ALLEG_DEPS_LIBS "${CMAKE_BINARY_DIR}/allegro5/deps/lib/")

	set(ZLIB_LIBRARY "${ALLEG_DEPS_LIBS}/zlib.lib" CACHE STRING "" FORCE)
	set(ZLIB_INCLUDE_DIR "${ALLEG_DEPS_INCLUDE}" CACHE STRING "" FORCE)
	set(PNG_LIBRARY "${ALLEG_DEPS_LIBS}/libpng16.lib" CACHE STRING "" FORCE)
	set(PNG_PNG_INCLUDE_DIR "${ALLEG_DEPS_INCLUDE}/libpng16" CACHE STRING "" FORCE)
	set(JPEG_LIBRARY "${ALLEG_DEPS_LIBS}/jpeg.lib" CACHE STRING "" FORCE)
	set(JPEG_INCLUDE_DIR "${ALLEG_DEPS_INCLUDE}" CACHE STRING "" FORCE)
	set(FREETYPE_LIBRARY "${ALLEG_DEPS_LIBS}/freetype.lib" CACHE STRING "" FORCE)
	set(FREETYPE_INCLUDE_DIRS "${ALLEG_DEPS_INCLUDE}/freetype2/freetype" CACHE STRING "" FORCE)
	set(FREETYPE_INCLUDE_DIR_freetype2 "${ALLEG_DEPS_INCLUDE}/freetype2/freetype" CACHE STRING "" FORCE)
	set(FREETYPE_INCLUDE_DIR_ft2build "${ALLEG_DEPS_INCLUDE}/freetype2" CACHE STRING "" FORCE)
	set(FREETYPE_ZLIB ON CACHE BOOL "" FORCE)
	set(FREETYPE_PNG ON CACHE BOOL "" FORCE)
	set(FREETYPE_BZIP2 OFF CACHE BOOL "" FORCE)
	set(OGG_LIBRARY "${ALLEG_DEPS_LIBS}/ogg.lib" CACHE STRING "" FORCE)
	set(OGG_INCLUDE_DIR "${ALLEG_DEPS_INCLUDE}" CACHE STRING "" FORCE)
	set(FLAC_LIBRARY "${ALLEG_DEPS_LIBS}/FLAC.lib" CACHE STRING "" FORCE)
	set(FLAC_INCLUDE_DIR "${ALLEG_DEPS_INCLUDE}" CACHE STRING "" FORCE)
	set(DUMB_LIBRARY "${ALLEG_DEPS_LIBS}/dumb.lib" CACHE STRING "" FORCE)
	set(DUMB_INCLUDE_DIR "${ALLEG_DEPS_INCLUDE}" CACHE STRING "" FORCE)
	set(MINIMP3_INCLUDE_DIRS "${ALLEG_DEPS_INCLUDE}" CACHE STRING "" FORCE)
endif (WIN32)

# Enable TLS. On Windows, build mbedTLS
set(USE_TLS ON CACHE BOOL "Enable TLS support" FORCE)
if (WIN32 AND USE_TLS)
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f mbedtls-3.4.0)
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls-3.4.0.tar.gz)
    set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
    set(MBEDTLS_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/mbedtls-3.4.0/include)
    set(MBEDTLS_VERSION_GREATER_THAN_3 ${MBEDTLS_INCLUDE_DIRS})
    set(MBEDTLS_LIBRARY mbedtls)
    set(MBEDX509_LIBRARY mbedx509)
    set(MBEDCRYPTO_LIBRARY mbedcrypto)
    add_subdirectory(${CMAKE_BINARY_DIR}/mbedtls-3.4.0 mbedtls_bin)
endif (WIN32 AND USE_TLS)

set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(IXWebSocket)
include_directories(IXWebSocket)
target_link_libraries(APCpp ixwebsocket)

if (MINGW)
    target_link_libraries(APCpp -static -static-libstdc++ -static-libgcc)
endif(MINGW)

# Attempt finding system version of JsonCpp
find_package(PkgConfig)
if (PKGCONFIG_FOUND AND (NOT MINGW))
    pkg_check_modules(JSONCPP jsoncpp)
    if (JSONCPP_FOUND)
        include_directories(${JSONCPP_INCLUDE_DIRS})
        target_link_libraries(APCpp ${JSONCPP_LDFLAGS})
    endif(JSONCPP_FOUND)
endif(PKGCONFIG_FOUND AND (NOT MINGW))

# No system version: Enable bundled
if((NOT JSONCPP_FOUND))
    set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(jsoncpp)
    set_property(TARGET jsoncpp_static PROPERTY POSITION_INDEPENDENT_CODE ON)
    include_directories(jsoncpp/include)
    target_link_libraries(APCpp jsoncpp_static)
endif((NOT JSONCPP_FOUND))

if (WIN32)
    target_link_libraries(APCpp ws2_32)
endif(WIN32)

# APSudoku setup
project(APSudoku)
add_executable(APSudoku
	src/Main.cpp
	src/GUI.cpp
	src/Font.cpp
	src/Theme.cpp
	src/Network.cpp
	src/PuzzleGen.cpp
	src/Config.cpp
	src/Util.cpp
	src/SudokuGrid.cpp
	)
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

option(WANT_TESTS "" OFF)
option(WANT_EXAMPLES "" OFF)
option(WANT_DEMO "" OFF)
option(WANT_DOCS "" OFF)
option(WANT_IMAGE_WEBP "" OFF)
option(WANT_OPUS "" OFF)
option(WANT_VIDEO "" OFF)
option(WANT_FONT "" ON)
option(WANT_TTF "" ON)
option(WANT_PRIMITIVES "" ON)

add_subdirectory(allegro5)
set(CMAKE_CONFIGURATION_TYPES Release Debug RelWithDebInfo CACHE STRING INTERNAL FORCE)
target_include_directories(APSudoku PUBLIC
	allegro5/include
	build/allegro5/include
	allegro5/addons/primitives
	allegro5/addons/font
	allegro5/addons/main
	allegro5/addons/color
	allegro5/addons/ttf
	)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT APSudoku)

target_link_libraries(APSudoku PUBLIC APCpp allegro allegro_main allegro_primitives allegro_font allegro_ttf)

add_custom_command(TARGET APSudoku POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:APSudoku>/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/out_$<CONFIG>/assets"
	COMMENT "Copying Assets")
if (WIN32)
	add_custom_command(TARGET APSudoku POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			"$<TARGET_FILE_DIR:APSudoku>/allegro_font-5.2.dll"
			"$<TARGET_FILE_DIR:APSudoku>/allegro_primitives-5.2.dll"
			"$<TARGET_FILE_DIR:APSudoku>/allegro_ttf-5.2.dll"
			"$<TARGET_FILE_DIR:APSudoku>/allegro-5.2.dll"
			"$<TARGET_FILE_DIR:APSudoku>/APCpp.dll"
			"$<TARGET_FILE:APSudoku>"
				"${CMAKE_BINARY_DIR}/out_$<CONFIG>"
	COMMENT "Packaging output")
endif (WIN32)
if (UNIX AND NOT APPLE)
	add_custom_command(TARGET APSudoku POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_BINARY_DIR}/lib/liballegro_font.so.5.2"
			"${CMAKE_BINARY_DIR}/lib/liballegro_primitives.so.5.2"
			"${CMAKE_BINARY_DIR}/lib/liballegro_ttf.so.5.2"
			"${CMAKE_BINARY_DIR}/lib/liballegro.so.5.2"
			"${CMAKE_BINARY_DIR}/libAPCpp.so"
			"$<TARGET_FILE:APSudoku>"
				"${CMAKE_BINARY_DIR}/out_$<CONFIG>"
	COMMENT "Packaging output")
endif (UNIX AND NOT APPLE)
