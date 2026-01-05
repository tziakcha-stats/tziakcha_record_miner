include(CTest)
enable_testing()

if(BUILD_TESTING)
	add_subdirectory(unit)
	add_subdirectory(scripts)
endif()
