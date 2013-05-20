cmake_minimum_required(VERSION 2.6)
project(cbdf)

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

FIND_PACKAGE(Boost 1.41 REQUIRED COMPONENTS iostreams)
FIND_PACKAGE(LibLZMA)
FIND_PACKAGE(LZO)


INCLUDE_DIRECTORIES(${INCLUDE_DIRECTORIES} ${LibLZMA_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/include)
LINK_DIRECTORIES(${LINK_DIRECTORIES} ${LibLZMA_INCLUDE_DIRS} ${Boost_LIBRARY_DIRS})

if(DEFINED CMAKE_INSTALL_PREFIX)
	message(STATUS "Prefix defined ${CMAKE_INSTALL_PREFIX}")
endif()

if(Boost_VERSION VERSION_GREATER 1.41)
	message(STATUS "Boost with UUID support found enabling boost::uuid")
	add_definitions(-DWITH_BOOST_UUID)
else()
	message(STATUS "Boost < 1.42 found, searching for libossp-uuid++")
	FIND_PACKAGE(UUID-C++ REQUIRED)
endif()

if(LIBLZMA_FOUND)
        message(STATUS "Found XZ compression library, enabling support")
        add_definitions(-DWITH_LZMA)
else()
        message(STATUS "XZ compression library not found, disabling support")
endif()

if(LZO_FOUND)
        message(STATUS "Found LZO compression library, enabling support")
        add_definitions(-DWITH_LZO)
else()
	message(STATUS "LZO compression library not found, disabling support")
endif()

add_subdirectory(src)

