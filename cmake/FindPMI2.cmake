# Output variables:
#  PMI2_INCLUDE_DIR : e.g., /usr/include/.
#  PMI2_LIBRARY     : Library path of PMI2 library
#  PMI2_FOUND       : True if found.


find_path(PMI2_INCLUDE_DIR
  NAMES pmi2.h
  PATH_SUFFIXES slurm
  PATHS ${PMI2_INCLUDE_DIRS}
)

find_library(PMI2_LIBRARY
  NAMES pmi2
  PATHS ${PMI2_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMI2
  REQUIRED_VARS
    PMI2_LIBRARY
    PMI2_INCLUDE_DIR
  VERSION_VAR PMI2_VERSION
)

if(PMI2_FOUND AND NOT TARGET PMI2::libpmi2)
  add_library(PMI2::libpmi2 UNKNOWN IMPORTED)
  set_target_properties(PMI2::libpmi2 PROPERTIES
    IMPORTED_LOCATION "${PMI2_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${PMI2_INCLUDE_DIR}"
  )
endif()
