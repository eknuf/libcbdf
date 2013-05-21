# Find liblzma
# XZ_FOUND - system has the XZ library
# XZ_INCLUDE_DIR - the XZ include directory
# XZ_LIBRARIES - The libraries needed to use XZ

if (XZ_INCLUDE_DIR AND XZ_LIBRARIES)
	# in cache already
	SET(XZ_FOUND TRUE)
else (XZ_INCLUDE_DIR AND XZ_LIBRARIES)
	FIND_PATH(XZ_INCLUDE_DIR lzma.h
		 ${XZ_ROOT}/include/
		 /usr/include/
		 /usr/local/include/
	)

	FIND_LIBRARY(XZ_LIBRARIES NAMES lzma
		PATHS
		${XZ_ROOT}/lib
		${XZ_ROOT}/lib64
		/usr/lib
		/usr/lib64
		/usr/local/lib
		/usr/local/lib64
		)

	if (XZ_INCLUDE_DIR AND XZ_LIBRARIES)
		 set(XZ_FOUND TRUE)
	endif (XZ_INCLUDE_DIR AND XZ_LIBRARIES)

	if (XZ_FOUND)
		 if (NOT XZ_FIND_QUIETLY)
				message(STATUS "Found XZ: ${XZ_LIBRARIES}")
		 endif (NOT XZ_FIND_QUIETLY)
	else (XZ_FOUND)
		 if (XZ_FIND_REQUIRED)
				message(FATAL_ERROR "Could NOT find XZ")
		 endif (XZ_FIND_REQUIRED)
	endif (XZ_FOUND)

	MARK_AS_ADVANCED(XZ_INCLUDE_DIR XZ_LIBRARIES)
endif (XZ_INCLUDE_DIR AND XZ_LIBRARIES)

