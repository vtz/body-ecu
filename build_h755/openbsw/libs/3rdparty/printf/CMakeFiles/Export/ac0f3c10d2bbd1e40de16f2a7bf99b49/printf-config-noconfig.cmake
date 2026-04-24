#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "printf::printf" for configuration ""
set_property(TARGET printf::printf APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(printf::printf PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libprintf.a"
  )

list(APPEND _cmake_import_check_targets printf::printf )
list(APPEND _cmake_import_check_files_for_printf::printf "${_IMPORT_PREFIX}/lib/libprintf.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
