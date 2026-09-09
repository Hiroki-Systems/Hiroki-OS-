#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Calamares::calamares" for configuration "Release"
set_property(TARGET Calamares::calamares APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Calamares::calamares PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "Qt6::DBus;Python::Python;Qt6::Xml"
  IMPORTED_LOCATION_RELEASE "/usr/lib/libcalamares.so.3.3.14"
  IMPORTED_SONAME_RELEASE "libcalamares.so.3.3"
  )

list(APPEND _cmake_import_check_targets Calamares::calamares )
list(APPEND _cmake_import_check_files_for_Calamares::calamares "/usr/lib/libcalamares.so.3.3.14" )

# Import target "Calamares::calamaresui" for configuration "Release"
set_property(TARGET Calamares::calamaresui APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Calamares::calamaresui PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "KF6::CoreAddons"
  IMPORTED_LOCATION_RELEASE "/usr/lib/libcalamaresui.so.3.3.14"
  IMPORTED_SONAME_RELEASE "libcalamaresui.so.3.3"
  )

list(APPEND _cmake_import_check_targets Calamares::calamaresui )
list(APPEND _cmake_import_check_files_for_Calamares::calamaresui "/usr/lib/libcalamaresui.so.3.3.14" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
