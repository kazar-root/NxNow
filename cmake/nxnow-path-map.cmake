# Record source paths as "NxNow/..." instead of the absolute checkout path.
# __FILE__ (used by assert and library logging) would otherwise embed the
# builder's local directory, including user and folder names, in the NRO.
# Included by the top-level project and injected into every ExternalProject
# through CMAKE_PROJECT_INCLUDE; add_compile_options quotes paths with spaces.
if(DEFINED NXNOW_PATH_MAP_FROM AND NOT NXNOW_PATH_MAP_FROM STREQUAL "")
    add_compile_options("-ffile-prefix-map=${NXNOW_PATH_MAP_FROM}=NxNow")
endif()
