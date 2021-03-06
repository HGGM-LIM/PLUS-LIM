PROJECT(PlusLib)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0020 NEW)
endif(COMMAND cmake_policy)

# ------------------------------------------------------------------------
# Set project version number 
SET(PLUSLIB_VERSION_MAJOR "2")
SET(PLUSLIB_VERSION_MINOR "3")
SET(PLUSLIB_VERSION_PATCH "0")
SET(PLUSLIB_VERSION ${PLUSLIB_VERSION_MAJOR}.${PLUSLIB_VERSION_MINOR}.${PLUSLIB_VERSION_PATCH})

IF ( ${CMAKE_GENERATOR} MATCHES "Visual Studio" ) 
  IF( ${CMAKE_GENERATOR} MATCHES Win64 )
    SET(PLUSLIB_PLATFORM "Win64" CACHE STRING "Name of the platform this instance of PlusLib is built on")
  ELSE()
    SET(PLUSLIB_PLATFORM "Win32" CACHE STRING "Name of the platform this instance of PlusLib is built on")
  ENDIF()
ELSE()
  SET (PLUSLIB_PLATFORM "Linux/Mac" CACHE STRING "Name of the platform this instance of PlusLib is built on")
ENDIF()

# --------------------------------------------------------------------------
# System Settings
IF(${WIN32})
  #this method adds the necessary compiler flag
  SET (RUNTIME_MINIMUM_WINDOWS_VERSION "0x0602")
ENDIF()

# --------------------------------------------------------------------------
# User options
#

# Standard CMake option for testing.
OPTION(BUILD_TESTING
       "Build test programs"
       ON)
       
# --------------------------------------------------------------------------
# Testing
#

IF (PLUS_OFFLINE_BUILD)
  SET(PLUSLIB_REVISION "x")
ELSE(PLUS_OFFLINE_BUILD)
  # Need subversion for the automatic update of the repository
  find_package(Subversion)
  IF(Subversion_FOUND)
    Subversion_WC_INFO(${CMAKE_CURRENT_SOURCE_DIR} PLUSLIB_SVN_INFO)
    MESSAGE(STATUS "Current revision is ${PLUSLIB_SVN_INFO_WC_REVISION}") 
    SET(PLUSLIB_REVISION ${PLUSLIB_SVN_INFO_WC_REVISION})
  ENDIF(Subversion_FOUND)
  SET( PLUSLIB_SVN_EXECUTABLE ${Subversion_SVN_EXECUTABLE} )
ENDIF(PLUS_OFFLINE_BUILD)

INCLUDE(CTest)

# --------------------------------------------------------------------------
# Set up paths for data and SDKs
#

SET( PLUSLIB_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
SET( PLUSLIB_TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools)

SET( ATC_TRAKSTAR_BASE_DIR ${PLUSLIB_TOOLS_DIR}/Ascension/trakSTAR_940041_RevE)
SET( ATC_TRAKSTAR_INCLUDE_DIR ${ATC_TRAKSTAR_BASE_DIR}/inc CACHE PATH "Path to the Ascension trakSTAR 3D Guidance include folder (where the ATC3DG.h file is located)" FORCE )
SET( ATC_TRAKSTAR_LIB_DIR ${ATC_TRAKSTAR_BASE_DIR}/lib CACHE PATH "Path to the Ascension trakSTAR 3D Guidance lib folder" FORCE )
SET( ATC_TRAKSTAR_BINARY_DIR ${ATC_TRAKSTAR_BASE_DIR}/bin CACHE PATH "Path to the Ascension trakSTAR 3D Guidance dll folder" FORCE ) 
SET( ATC_MEDSAFE_BASE_DIR ${PLUSLIB_TOOLS_DIR}/Ascension/medSAFE_940033_Rev_F)
SET( ATC_MEDSAFE_INCLUDE_DIR ${ATC_MEDSAFE_BASE_DIR}/inc CACHE PATH "Path to the Ascension medSAFE 3D Guidance include folder (where the ATC3DGm.h file is located)" FORCE )
SET( ATC_MEDSAFE_LIB_DIR ${ATC_MEDSAFE_BASE_DIR}/lib CACHE PATH "Path to the Ascension medSAFE 3D Guidance lib folder" FORCE )
SET( ATC_MEDSAFE_BINARY_DIR ${ATC_MEDSAFE_BASE_DIR}/bin CACHE PATH "Path to the Ascension medSAFE 3D Guidance dll folder" FORCE ) 

SET( PHIDGET_SPATIAL_BASE_DIR ${PLUSLIB_TOOLS_DIR}/Phidget/PhidgetSpatial-2.1.8)  
SET( PHIDGET_SPATIAL_INCLUDE_DIR ${PHIDGET_SPATIAL_BASE_DIR} CACHE PATH "Path to the Phidget Spatial include folder" FORCE )
SET( PHIDGET_SPATIAL_WIN32_LIB_DIR ${PHIDGET_SPATIAL_BASE_DIR}/x86 CACHE PATH "Path to the Phidget Spatial 32-bit lib folder" FORCE )
SET( PHIDGET_SPATIAL_WIN32_BINARY_DIR ${PHIDGET_SPATIAL_BASE_DIR}/x86 CACHE PATH "Path to the Phidget Spatial 32-bit dll folder" FORCE ) 
SET( PHIDGET_SPATIAL_WIN64_LIB_DIR ${PHIDGET_SPATIAL_BASE_DIR}/x64 CACHE PATH "Path to the Phidget Spatial 64-bit lib folder" FORCE )
SET( PHIDGET_SPATIAL_WIN64_BINARY_DIR ${PHIDGET_SPATIAL_BASE_DIR}/x64 CACHE PATH "Path to the Phidget Spatial 64-bit dll folder" FORCE ) 

SET( EPIPHAN_SDK_BASE_DIR ${PLUSLIB_TOOLS_DIR}/Epiphan )
SET( EPIPHAN_INCLUDE_DIR ${EPIPHAN_SDK_BASE_DIR} CACHE PATH "Path to the Epiphan SDK include directory." FORCE )
SET( EPIPHAN_WIN32_BINARY_DIR ${EPIPHAN_SDK_BASE_DIR}/Win32 CACHE PATH "Path to the Epiphan 32-bit dll folder" FORCE )
SET( EPIPHAN_WIN32_LIB_DIR ${EPIPHAN_SDK_BASE_DIR}/Win32 CACHE PATH "Path to the Epiphan 32-bit lib folder" FORCE )
SET( EPIPHAN_WIN64_BINARY_DIR ${EPIPHAN_SDK_BASE_DIR}/x64 CACHE PATH "Path to the Epiphan 64-bit dll folder" FORCE )
SET( EPIPHAN_WIN64_LIB_DIR ${EPIPHAN_SDK_BASE_DIR}/x64 CACHE PATH "Path to the Epiphan 64-bit lib folder" FORCE )

SET( USDIGITAL_SEI_BASE_DIR ${PLUSLIB_TOOLS_DIR}/UsDigital/SEI_5.22)
SET( USDIGITAL_SEI_INCLUDE_DIR ${USDIGITAL_SEI_BASE_DIR}/inc CACHE PATH "Path to the US Digital SEI include directory (needed for the CIVCO stepper)." FORCE )
SET( USDIGITAL_SEI_BINARY_DIR ${USDIGITAL_SEI_BASE_DIR}/bin CACHE PATH "Path to the US Digital SEI binary directory (needed for the CIVCO stepper)." FORCE )
SET( USDIGITAL_SEI_LIB_DIR ${USDIGITAL_SEI_BASE_DIR}/lib CACHE PATH "Path to the US Digital SEI library directory (needed for the CIVCO stepper)." FORCE )

# --------------------------------------------------------------------------
# Subdirs
#

ADD_SUBDIRECTORY(src)

#-----------------------------------------------------------------------------
# Generate convenience files for automatic build, test, and packaging
#

STRING(TOUPPER ${CMAKE_MAKE_PROGRAM} MAKE_PROGRAM_UPPERCASE)
IF (${MAKE_PROGRAM_UPPERCASE} MATCHES "MSBUILD")
  SET(IS_MAKE_PROGRAM_MSBUILD TRUE)
ELSE()
  SET(IS_MAKE_PROGRAM_MSBUILD FALSE)
ENDIF()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/BuildAndTest.bat.in 
  ${CMAKE_CURRENT_BINARY_DIR}/BuildAndTest.bat
  )
  
# Documentation generator
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/CreateApiReference.bat.in 
  ${CMAKE_CURRENT_BINARY_DIR}/CreateApiReference.bat
  )

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/BuildAndTest.sh.in 
  ${CMAKE_CURRENT_BINARY_DIR}/BuildAndTest.sh
  )
     
