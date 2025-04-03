# - Find mimalloc library
# Once done this will define
#  MIMALLOC_FOUND - System has mimalloc
#  MIMALLOC_INCLUDE_DIRS - The mimalloc include directories
#  MIMALLOC_LIBRARIES - The libraries needed to use mimalloc

find_path(MIMALLOC_INCLUDE_DIR mimalloc.h
          PATH_SUFFIXES mimalloc)

find_library(MIMALLOC_LIBRARY NAMES mimalloc libmimalloc)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set MIMALLOC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(mimalloc DEFAULT_MSG
                                  MIMALLOC_LIBRARY MIMALLOC_INCLUDE_DIR)

mark_as_advanced(MIMALLOC_INCLUDE_DIR MIMALLOC_LIBRARY)

set(MIMALLOC_LIBRARIES ${MIMALLOC_LIBRARY})
set(MIMALLOC_INCLUDE_DIRS ${MIMALLOC_INCLUDE_DIR})
