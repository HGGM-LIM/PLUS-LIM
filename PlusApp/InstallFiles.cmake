# --------------------------------------------------------------------------
# Install
# 

IF(BUILD_DOCUMENTATION)  
  IF(WIN32)
    SET(COMPRESSED_HELP_TARGET_FILE_EXT ".chm")
  ELSE()
    SET(COMPRESSED_HELP_TARGET_FILE_EXT ".tar.gz")
  ENDIF()
  INSTALL(FILES 
    ${PLUS_EXECUTABLE_OUTPUT_PATH}/Doc/PlusApp-UserManual${COMPRESSED_HELP_TARGET_FILE_EXT}
    DESTINATION ${PLUSAPP_INSTALL_DOCUMENTATION_DIR}
    COMPONENT Documentation
    )
ENDIF(BUILD_DOCUMENTATION)

SET ( PLUSLIB_CONFIG_FILES
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Sim_PivotCalibration.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Sim_RecordPhantomLandmarks.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Sim_SpatialCalibration_2.0.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Sim_TemporalCalibration.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Sim_VolumeReconstruction.xml

  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_ChRobotics.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Microchip.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Sim_NwirePhantom.xml  
  ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_SimulatedUltrasound_3DSlicer.xml
  ${PLUSLIB_DATA_DIR}/ConfigFiles/SimulatedUltrasound_GelBlockModel_Reference.stl
  ${PLUSLIB_DATA_DIR}/ConfigFiles/SimulatedUltrasound_NeedleModel_NeedleTip.stl
  ${PLUSLIB_DATA_DIR}/ConfigFiles/SimulatedUltrasound_VesselModel_Reference.stl
  ${PLUSLIB_DATA_DIR}/ConfigFiles/SimulatedUltrasound_Scene.mrb

  ${PLUSLIB_DATA_DIR}/CADModels/fCalPhantom/fCal_1.0.stl
  ${PLUSLIB_DATA_DIR}/CADModels/fCalPhantom/fCal_1.2.stl
  ${PLUSLIB_DATA_DIR}/CADModels/fCalPhantom/fCal_2.0.stl
  ${PLUSLIB_DATA_DIR}/CADModels/fCalPhantom/fCal_3.1.stl
  ${PLUSLIB_DATA_DIR}/CADModels/fCalPhantom/fCal_L1.4.stl
  ${PLUSLIB_DATA_DIR}/CADModels/LinearProbe/Probe_L14-5_38.stl
  ${PLUSLIB_DATA_DIR}/CADModels/EndocavityProbe/Probe_EC9-5_10.stl
  ${PLUSLIB_DATA_DIR}/CADModels/CurvilinearProbe/Probe_C5-2_60.stl 
  ${PLUSLIB_DATA_DIR}/CADModels/Stylus/Stylus_Example.stl
)

IF(PLUS_USE_3dConnexion_TRACKER)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_3dConnexion.xml
    )
ENDIF(PLUS_USE_3dConnexion_TRACKER)

IF(PLUS_USE_Ascension3DG)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Ascension3DG.xml
    )
ENDIF(PLUS_USE_Ascension3DG)

IF(PLUS_USE_BKPROFOCUS_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_BkProFocus_OpenIGTLinkTracker.xml
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_BkProFocusOem.xml 
    )  
  SET (BK_SETTINGS
    ${PLUSLIB_DATA_DIR}/ConfigFiles/BkSettings/IniFile.ccf
    ${PLUSLIB_DATA_DIR}/ConfigFiles/BkSettings/IniFile.ini
    )
ENDIF(PLUS_USE_BKPROFOCUS_VIDEO)

IF(PLUS_USE_IntuitiveDaVinci)
  SET(PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/daVinci/daVinci.xml
    )
ENDIF(PLUS_USE_IntuitiveDaVinci)

IF(PLUS_USE_EPIPHAN)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_EpiphanVideoCapture.xml
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_EpiphanColorVideoCapture.xml
    )
  IF(PLUS_USE_POLARIS)
    SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Epiphan_NDIPolaris.xml
      )    
  ENDIF(PLUS_USE_POLARIS)
ENDIF(PLUS_USE_EPIPHAN)

IF(PLUS_USE_ICCAPTURING_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_ImagingControlsVideoCapture.xml
    )
ENDIF(PLUS_USE_ICCAPTURING_VIDEO)

IF(PLUS_USE_INTERSON_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_IntersonVideoCapture.xml
    )
ENDIF(PLUS_USE_INTERSON_VIDEO)

IF(PLUS_USE_MICRONTRACKER)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_MicronTracker.xml
    ${PLUSLIB_DATA_DIR}/ConfigFiles/MicronTracker.ini
    )
  SET ( MICRONTRACKER_TOOL_DEFINITIONS 
    ${PLUSLIB_DATA_DIR}/ConfigFiles/Markers/1b
    ${PLUSLIB_DATA_DIR}/ConfigFiles/Markers/2b
    ${PLUSLIB_DATA_DIR}/ConfigFiles/Markers/a
    ${PLUSLIB_DATA_DIR}/ConfigFiles/Markers/COOLCARD
    ${PLUSLIB_DATA_DIR}/ConfigFiles/Markers/TTblock
    )
  IF(PLUS_USE_ULTRASONIX_VIDEO)
    SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Ultrasonix_L14-5_MicronTracker_2.0.xml
      )  
  ENDIF(PLUS_USE_ULTRASONIX_VIDEO)  
ENDIF(PLUS_USE_MICRONTRACKER)

IF(PLUS_USE_MMF_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_MmfVideoCapture.xml
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_MmfColorVideoCapture.xml
    )  
ENDIF(PLUS_USE_MMF_VIDEO)

IF(PLUS_USE_PHIDGET_SPATIAL_TRACKER)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_PhidgetSpatial.xml
    )  
ENDIF(PLUS_USE_PHIDGET_SPATIAL_TRACKER)

IF(PLUS_USE_PHILIPS_3D_ULTRASOUND)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Philips_ie33_NDIAurora.xml
    )
ENDIF(PLUS_USE_PHILIPS_3D_ULTRASOUND)

IF(PLUS_USE_POLARIS)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Ultrasonix_L14-5_NDIPolaris_2.0.xml
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_NDIPolaris.xml
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_NDIAurora.xml
    )
  SET ( NDI_TOOL_DEFINITIONS 
    ${PLUSLIB_DATA_DIR}/ConfigFiles/NdiToolDefinitions/8700339.rom
    ${PLUSLIB_DATA_DIR}/ConfigFiles/NdiToolDefinitions/8700340.rom
    ${PLUSLIB_DATA_DIR}/ConfigFiles/NdiToolDefinitions/8700449.rom
    )
ENDIF(PLUS_USE_POLARIS)

IF (PLUS_USE_STEALTHLINK)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_StealthLinkTracker.xml
    )
ENDIF(PLUS_USE_STEALTHLINK)

IF(PLUS_USE_TELEMED_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_TelemedVideoCapture.xml
    )
ENDIF(PLUS_USE_TELEMED_VIDEO)


IF(PLUS_USE_THORLABS_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_ThorLabsVideoCapture.xml
    )
ENDIF(PLUS_USE_THORLABS_VIDEO)

IF(PLUS_USE_ULTRASONIX_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Ultrasonix_L14-5.xml
    )
  IF(PLUS_USE_Ascension3DG)
    SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Ultrasonix_L14-5_Ascension3DG_2.0.xml
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Ultrasonix_L14-5_Ascension3DG_3.0.xml
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_fCal_Ultrasonix_L14-5_Ascension3DG_L1.4.xml
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Ultrasonix_L14-5_Ascension3DG_calibrated.xml
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Ultrasonix_C5-2_Ascension3DG_calibrated.xml
      ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_Ultrasonix_4DL14-5_Porta_calibrated.xml
      )
  ENDIF(PLUS_USE_Ascension3DG)
ENDIF(PLUS_USE_ULTRASONIX_VIDEO)

IF(PLUS_USE_VFW_VIDEO)
  SET ( PLUSLIB_CONFIG_FILES ${PLUSLIB_CONFIG_FILES}
    ${PLUSLIB_DATA_DIR}/ConfigFiles/PlusDeviceSet_Server_VfwVideoCapture.xml
    )
ENDIF(PLUS_USE_VFW_VIDEO)


SET ( PLUSLIB_DATA_FILES
  ${PLUSLIB_DATA_DIR}/TestImages/fCal_Test_Calibration_3NWires.mha
  ${PLUSLIB_DATA_DIR}/TestImages/fCal_Test_Calibration_3NWires_fCal2.0.mha
  ${PLUSLIB_DATA_DIR}/TestImages/WaterTankBottomTranslationTrackerBuffer-trimmed.mha
  ${PLUSLIB_DATA_DIR}/TestImages/WaterTankBottomTranslationVideoBuffer.mha
  ${PLUSLIB_DATA_DIR}/TestImages/EightLandmarkPointsTrackedForPhantomRegistration.mha
)

# Install Qt libs
IF( ${QT_VERSION_MAJOR} EQUAL 5 )
  INSTALL(FILES 
    ${QT_BINARY_DIR}/Qt5Core${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5Gui${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5Network${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5Sql${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5WebKit${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5XmlPatterns${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5OpenGL${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5Widgets${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/Qt5Xml${CMAKE_SHARED_LIBRARY_SUFFIX}
    DESTINATION ${PLUSAPP_INSTALL_BIN_DIR}
    COMPONENT RuntimeLibraries
  )
  INSTALL(FILES 
    ${CMAKE_PREFIX_PATH}/plugins/platforms/qwindows${CMAKE_SHARED_LIBRARY_SUFFIX}
    DESTINATION ${PLUSAPP_INSTALL_BIN_DIR}/platforms
    COMPONENT RuntimeLibraries
  )
ELSE()
  INSTALL(FILES 
    ${QT_BINARY_DIR}/QtCore4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtGui4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtNetwork4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtSql4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtWebKit4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtXmlPatterns4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/phonon4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtOpenGL4${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${QT_BINARY_DIR}/QtXml4${CMAKE_SHARED_LIBRARY_SUFFIX}
    DESTINATION ${PLUSAPP_INSTALL_BIN_DIR}
    COMPONENT RuntimeLibraries
  )
ENDIF()

IF (WIN32)
  # Install Plus command prompt starting script
  INSTALL(FILES 
    ${PLUSLIB_SOURCE_DIR}/src/scripts/StartPlusCommandPrompt.bat
    ${PLUSLIB_SOURCE_DIR}/src/scripts/StartPlusCommandPrompt.ico
    DESTINATION ${PLUSAPP_INSTALL_BIN_DIR}
    COMPONENT RuntimeLibraries
  )
ENDIF (WIN32)

IF(PLUSAPP_INSTALL_CONFIG_DIR)
  INSTALL(FILES 
    ${PLUSLIB_CONFIG_FILES}
    DESTINATION ${PLUSAPP_INSTALL_CONFIG_DIR}
    COMPONENT RuntimeLibraries
    )
  IF(PLUS_USE_POLARIS)
    INSTALL(FILES 
      ${NDI_TOOL_DEFINITIONS}
      DESTINATION ${PLUSAPP_INSTALL_CONFIG_DIR}/NdiToolDefinitions
      COMPONENT RuntimeLibraries
      )
  ENDIF(PLUS_USE_POLARIS)
  IF(PLUS_USE_BKPROFOCUS_VIDEO)
    INSTALL(FILES 
      ${BK_SETTINGS}
      DESTINATION ${PLUSAPP_INSTALL_CONFIG_DIR}/BkSettings
      COMPONENT RuntimeLibraries
      )
  ENDIF(PLUS_USE_BKPROFOCUS_VIDEO)
  IF(PLUS_USE_MICRONTRACKER)
    INSTALL(FILES 
      ${MICRONTRACKER_TOOL_DEFINITIONS}
      DESTINATION ${PLUSAPP_INSTALL_CONFIG_DIR}/Markers
      COMPONENT RuntimeLibraries
      )  
  ENDIF(PLUS_USE_MICRONTRACKER)
ENDIF()

IF(PLUSAPP_INSTALL_DATA_DIR)
  INSTALL(FILES 
    ${PLUSLIB_DATA_FILES}
    DESTINATION ${PLUSAPP_INSTALL_DATA_DIR}
    COMPONENT RuntimeLibraries
    )
ENDIF()

INSTALL(FILES 
  ${CMAKE_CURRENT_BINARY_DIR}/PlusConfig.xml
  DESTINATION ${PLUSAPP_INSTALL_BIN_DIR}
  COMPONENT RuntimeLibraries
)
