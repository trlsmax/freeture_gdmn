#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "opencv_core" for configuration "Debug"
set_property(TARGET opencv_core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(opencv_core PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_core450d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_core450d.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS opencv_core )
list(APPEND _IMPORT_CHECK_FILES_FOR_opencv_core "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_core450d.lib" "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_core450d.dll" )

# Import target "opencv_imgproc" for configuration "Debug"
set_property(TARGET opencv_imgproc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(opencv_imgproc PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_imgproc450d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_imgproc450d.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS opencv_imgproc )
list(APPEND _IMPORT_CHECK_FILES_FOR_opencv_imgproc "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_imgproc450d.lib" "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_imgproc450d.dll" )

# Import target "opencv_video" for configuration "Debug"
set_property(TARGET opencv_video APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(opencv_video PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_video450d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_video450d.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS opencv_video )
list(APPEND _IMPORT_CHECK_FILES_FOR_opencv_video "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_video450d.lib" "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_video450d.dll" )

# Import target "opencv_imgcodecs" for configuration "Debug"
set_property(TARGET opencv_imgcodecs APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(opencv_imgcodecs PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_imgcodecs450d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_imgcodecs450d.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS opencv_imgcodecs )
list(APPEND _IMPORT_CHECK_FILES_FOR_opencv_imgcodecs "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_imgcodecs450d.lib" "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_imgcodecs450d.dll" )

# Import target "opencv_videoio" for configuration "Debug"
set_property(TARGET opencv_videoio APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(opencv_videoio PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_videoio450d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_videoio450d.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS opencv_videoio )
list(APPEND _IMPORT_CHECK_FILES_FOR_opencv_videoio "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_videoio450d.lib" "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_videoio450d.dll" )

# Import target "opencv_highgui" for configuration "Debug"
set_property(TARGET opencv_highgui APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(opencv_highgui PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_highgui450d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_highgui450d.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS opencv_highgui )
list(APPEND _IMPORT_CHECK_FILES_FOR_opencv_highgui "${_IMPORT_PREFIX}/x64/vc16/lib/opencv_highgui450d.lib" "${_IMPORT_PREFIX}/x64/vc16/bin/opencv_highgui450d.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
