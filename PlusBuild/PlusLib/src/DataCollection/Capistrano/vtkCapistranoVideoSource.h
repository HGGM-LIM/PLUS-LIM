/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkCapistranoVideoSource_h
#define __vtkCapistranoVideoSource_h

#include "vtkDataCollectionExport.h"
#include "vtkPlusDevice.h"
#include "vtkUsImagingParameters.h"

/*!
 \class vtkCapistranoVideoSource
 \brief Class for acquiring ultrasound images from Capistrano Labs USB ultrasound systems.

 Requires PLUS_USE_CAPISTRANO_VIDEO option in CMake.
 Requires the Capistrano cSDK2013 (SDK provided by Capistrano Labs).

 \ingroup PlusLibDataCollection.
*/
class vtkDataCollectionExport vtkCapistranoVideoSource: public vtkPlusDevice
{
public:
  /*! Constructor for a smart pointer of this class*/
  static vtkCapistranoVideoSource * New();
  vtkTypeMacro(vtkCapistranoVideoSource, vtkPlusDevice);
  void PrintSelf(ostream& os, vtkIndent indent);

  /*! Specify the device connected to this class */
  virtual bool IsTracker() const { return false; }

  /*! Read configuration from xml data */
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* config);

  /*! Write configuration to xml data */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* config);

  /*! Verify the device is correctly configured */
  virtual PlusStatus NotifyConfigured();

  /*! Get the version of SDK */
  virtual std::string GetSdkVersion();

  /* Update Speed of Sound */
  PlusStatus GetProbeVelocityDevice(float& aVel);

  /* Set the speed of sound of US probe */
  PlusStatus SetSoundVelocity(float ss);

  /* Set the scan directional mode of US probe */
  PlusStatus SetBidirectionalMode(bool mode);

  /* Set the size of cinebuffer of US probe */
  PlusStatus SetCineBuffers(int cinebuffer);

  /* Set the sample frequency of US probe */
  PlusStatus SetSampleFrequency(float sf);

  /* Set the pulser frequency of US probe */
  PlusStatus SetPulseFrequency(float pf);

  /* Set the pulser voltage of US probe */
  PlusStatus SetPulseVoltage(float pv);

  /* Set the scan depth of US probe */
  PlusStatus SetScanDepth(float sd);

  /* Set the interpolation of B-Mode image */
  PlusStatus SetInterpolate(bool interpolate);

  /* Set the average mode of US B-Mode image */
  PlusStatus SetAverageMode(bool averagemode);

  /* Set the view option of US B-Mode image */
  PlusStatus SetBModeViewOption(unsigned int bmodeviewoption);

  /* Set the size of US B-mode image */
  PlusStatus SetImageSize(int imageSize[2]);

  /* Set the Intensity (Brightness) of US B-mode image */
  PlusStatus SetIntensity(double value);

  /* Set the Contrast of US B-mode image */
  PlusStatus SetContrast(double value);

  /* Set the zoom factor. */
  PlusStatus SetZoomFactor(double zoomfactor);

  /* Set the zoom factor on the US Device. */
  PlusStatus SetDisplayZoomDevice(double zoom);

  /* Set the LUT Center of US B-mode image */
  PlusStatus SetLutCenter(double lutcenter);

  /* Set the LUT Window of US B-mode image */
  PlusStatus SetLutWindow(double lutwindow);

  /* Set the gain in percent */
  PlusStatus SetGainPercent(double gainPercent[3]);

  /* Set the gain in percent in the device */
  PlusStatus SetGainPercentDevice(double gainPercent[3]);

  /* Set the probe depth on US Device in mm */
  PlusStatus SetDepthMmDevice(float depthMm);

  /* Set the probe depth in mm */
  PlusStatus SetDepthMm(float depthMm);

  /* Apply a completely new set of imaging parameters to the device */
  PlusStatus SetNewImagingParametersDevice(const vtkUsImagingParameters& newImagingParameters);
protected:

  /*! Constructor */
  vtkCapistranoVideoSource();

  /*! Destructor */
  ~vtkCapistranoVideoSource();

  /*! Device-specific connect */
  virtual PlusStatus InternalConnect();

  /*! Device-specific disconnect */
  virtual PlusStatus InternalDisconnect();

  /*! Device-specific recording start */
  virtual PlusStatus InternalStartRecording();

  /*! Device-specific recording stop */
  virtual PlusStatus InternalStopRecording();

  /*! Initialize vtkCapistranoVideoSource */
  PlusStatus InitializeCapistranoVideoSource(bool probeConnected = false);

  /*! The internal function which actually does the grab. */
  PlusStatus InternalUpdate();

  /*! Initialize a Capistrano Probe */
  PlusStatus InitializeCapistranoProbe();

  /* Set up US Probe with ID */
  PlusStatus SetupProbe(int probeID = 0);

  /*! Initialize a ImageWindow and vtkPlusDataSource */
  PlusStatus InitializeImageWindow();

  /*! Initialize an LUT for US B-Mode image */
  PlusStatus InitializeLUT();

  /*! Initialize an TGC for US B-Mode image */
  PlusStatus InitializeTGC();

  /*! Set ON/OFF of collecting US data. */
  PlusStatus FreezeDevice(bool freeze);

  /*! Wait US Data from US device  */
  PlusStatus WaitForFrame();

  /* Set the boolean value to use US parameters from XML file */
  PlusStatus SetUpdateParameters(bool b);

  /* Update US parameters (US probe/B-Mode parameters) */
  PlusStatus UpdateUSParameters();

  /* Update US probe parameters  */
  PlusStatus UpdateUSProbeParameters();

  /* Update US B-Mode parameters */
  PlusStatus UpdateUSBModeParameters();

  /* Calculate US Image Display */
  PlusStatus CalculateDisplay();

  /* Calculate US Image Display with a given B-Mode view option */
  PlusStatus CalculateDisplay(unsigned int option);

  /* Update US Scan depth */
  PlusStatus UpdateDepthMode();

  /* Update US Scan depth with a given clockdivider */
  PlusStatus UpdateDepthMode(int clockdivider);

  /* Update US Sample frequency */
  PlusStatus GetSampleFrequencyDevice(float& aFreq);

  /*! Get probe name from the device */
  PlusStatus GetProbeNameDevice(std::string& probeName);

  /*! For internal storage of additional variables
      (to minimize the number of included headers) */
  class        vtkInternal;
  vtkInternal* Internal;

  bool         Frozen;
  bool         UpdateParameters;
  bool         BidirectionalMode;
  int          ProbeID;
  int          ClockDivider;
  int          CineBuffers;
  float        SampleFrequency;
  float        PulseFrequency;
  bool         Interpolate;
  bool         AverageMode;
  unsigned int CurrentBModeViewOption;
  double       LutCenter;
  double       LutWindow;

private:
  vtkCapistranoVideoSource(const vtkCapistranoVideoSource &); // Not implemented
  void operator=(const vtkCapistranoVideoSource &); // Not implemented
};

#endif
