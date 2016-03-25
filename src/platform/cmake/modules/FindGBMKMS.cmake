#
# Used for non-X11 dependant EGL/GL,GLES video platform
#

find_path(DRM_INCLUDE_DIR drm.h
		PATHS
    /usr/local/include/libdrm
    /usr/local/include/drm
    /usr/include/libdrm
    /usr/include/drm
)

find_path(GBM_INCLUDE_DIR gbm.h
		PATHS
		/usr/local/include
		/usr/include
)

find_library(GBMKMS_DRM_LIBRARY NAMES drm REQUIRED)

if (NOT DRM_INCLUDE_DIR OR NOT GBMKMS_DRM_LIBRARY)
	if (GBMKMS_FIND_REQUIRED)
		message(FATAL_ERROR "Could not find libDRM")
	endif()
endif()

if (NOT IGNORE_GBM)
	find_library(GBMKMS_GBM_LIBRARY NAMES gbm)
	if (NOT GBMKMS_GBM_LIBRARY OR NOT GBM_INCLUDE_DIR)
		if (GBMKMS_FIND_REQUIRED)
			message(FATAL_ERROR "Could not find libGBM")
		endif()
	endif()
	set(GBMKMS_LIBRARIES
		${GBMKMS_DRM_LIBRARY}
		${GBMKMS_GBM_LIBRARY}
	)
	set(GBMKMS_INCLUDE_DIRS ${GBM_INCLUDE_DIR} ${DRM_INCLUDE_DIR})
else()
	set(GBMKMS_LIBRARIES ${GBMKMS_DRM_LIBRARY})
	set(GBMKMS_INCLUDE_DIRS ${DRM_INCLUDE_DIR})
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GBMKMS DEFAULT_MSG GBMKMS_LIBRARIES)
