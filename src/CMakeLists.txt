cmake_minimum_required(VERSION 2.6)

if(LIBLZMA_FOUND)
SET (CBDF_SOURCES cbdf.cpp lzma.cpp)
else()
SET (CBDF_SOURCES cbdf.cpp)
endif()

add_library(cbdf_static STATIC ${CBDF_SOURCES})
add_library(cbdf SHARED ${CBDF_SOURCES})

install(TARGETS cbdf_static DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64)
install(TARGETS cbdf DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64)

install(FILES include/cbdf.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
