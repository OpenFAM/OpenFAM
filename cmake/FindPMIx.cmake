# Output variables:
#  PMIx_INCLUDE_DIR : e.g., /usr/include/.
#  PMIx_LIBRARY     : Library path of PMIx library
#  PMIx_FOUND       : True if found.
#  PMIx_PMI2_FOUND  : True if pmi2.h found.

find_package(PkgConfig)
pkg_check_modules(PC_PMIx QUIET libpmix)

find_path(PMIx_INCLUDE_DIR
  NAMES pmix.h
  PATHS ${PC_PMIx_INCLUDE_DIRS}
)

find_library(PMIx_LIBRARY
  NAMES pmix
  PATHS ${PC_PMIx_LIBRARY_DIRS}
)

find_path(PMIx_PMI2_INCLUDE_DIR
  NAMES pmi2.h
  PATHS ${PC_PMIx_INCLUDE_DIRS}
)
if(PMIx_PMI2_INCLUDE_DIR)
  set(PMIx_PMI2_FOUND ON)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMIx
  REQUIRED_VARS
    PMIx_LIBRARY
    PMIx_INCLUDE_DIR
  VERSION_VAR PMIx_VERSION
)

if(PMIx_FOUND AND NOT TARGET PMIx::libpmix)
  add_library(PMIx::libpmix UNKNOWN IMPORTED)
  set_target_properties(PMIx::libpmix PROPERTIES
    IMPORTED_LOCATION "${PMIx_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_PMIx_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${PMIx_INCLUDE_DIR}"
  )
  if(PMIx_PMI2_FOUND AND NOT TARGET PMIx::pmi2)
    set_target_properties(PMIx::pmi2 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${PMIx_PMI2_INCLUDE_DIR}"
    )
  endif()
endif()

