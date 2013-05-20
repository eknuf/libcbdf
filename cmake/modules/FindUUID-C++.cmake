# Find libossp-uuid++
# UUID-C++_FOUND - system has the UUID-C++ library
# UUID-C++_INCLUDE_DIR - the UUID-C++ include directory
# UUID-C++_LIBRARIES - The libraries needed to use UUID-C++

if (UUID-C++_INCLUDE_DIR AND UUID-C++_LIBRARIES)
	# in cache already
	SET(UUID-C++_FOUND TRUE)
else (UUID-C++_INCLUDE_DIR AND UUID-C++_LIBRARIES)
	FIND_PATH(UUID-C++_INCLUDE_DIR uuid++.hh
		 ${UUID-C++_ROOT}/include/
		 /usr/include/
		 /usr/local/include/
	)

	FIND_LIBRARY(UUID-C++_LIBRARIES NAMES ossp-uuid++
		PATHS
		${UUID-C++_ROOT}/lib
		${UUID-C++_ROOT}/lib64
		/usr/lib
		/usr/lib64
		/usr/local/lib
		/usr/local/lib64
		)

	if (UUID-C++_INCLUDE_DIR AND UUID-C++_LIBRARIES)
		 set(UUID-C++_FOUND TRUE)
	endif (UUID-C++_INCLUDE_DIR AND UUID-C++_LIBRARIES)

	if (UUID-C++_FOUND)
		 if (NOT UUID-C++_FIND_QUIETLY)
				message(STATUS "Found UUID-C++: ${UUID-C++_LIBRARIES}")
		 endif (NOT UUID-C++_FIND_QUIETLY)
	else (UUID-C++_FOUND)
		 if (UUID-C++_FIND_REQUIRED)
				message(FATAL_ERROR "Could NOT find UUID-C++")
		 endif (UUID-C++_FIND_REQUIRED)
	endif (UUID-C++_FOUND)

	MARK_AS_ADVANCED(UUID-C++_INCLUDE_DIR UUID-C++_LIBRARIES)
endif (UUID-C++_INCLUDE_DIR AND UUID-C++_LIBRARIES)

