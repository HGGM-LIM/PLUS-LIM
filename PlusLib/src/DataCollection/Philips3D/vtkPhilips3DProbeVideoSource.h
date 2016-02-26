/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkPhilips3DProbeVideoSource_h
#define __vtkPhilips3DProbeVideoSource_h

#include "vtkDataCollectionExport.h"

#include "vtkPlusDevice.h"
#include "StreamMgr.h"

class vtkIEEListener;

/*!
\class vtkPhilips3DProbeVideoSource
\brief Class for providing VTK video input interface from Philips ie33 3D ultrasound probe
\ingroup PlusLibDataCollection
*/
class vtkDataCollectionExport vtkPhilips3DProbeVideoSource : public vtkPlusDevice
{
public:
  vtkTypeMacro(vtkPhilips3DProbeVideoSource, vtkPlusDevice);
  void PrintSelf(ostream& os, vtkIndent indent);   
  static vtkPhilips3DProbeVideoSource* New();

  /*! Read configuration from XML data */
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* config); 
  /*! Write configuration to XML data */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* config);

  /*! Respond to the query if this is a tracker or not */
  virtual bool IsTracker() const;

  /*! Perform any completion tasks once configured */
  virtual PlusStatus NotifyConfigured();

  vtkGetMacro(Port, int);
  vtkSetMacro(Port, int);

  vtkGetStringMacro(IPAddress);
  vtkSetStringMacro(IPAddress);

  vtkSetMacro(ForceZQuantize, bool);
  vtkGetMacro(ForceZQuantize, bool);

  vtkSetMacro(ResolutionFactor, double);
  vtkGetMacro(ResolutionFactor, double);

  vtkSetMacro(IntegerZ, bool);
  vtkGetMacro(IntegerZ, bool);

  vtkSetMacro(Isotropic, bool);
  vtkGetMacro(Isotropic, bool);

  vtkSetMacro(QuantizeDim, bool);
  vtkGetMacro(QuantizeDim, bool);

  vtkSetMacro(ZDecimation, int);
  vtkGetMacro(ZDecimation, int);

  vtkSetMacro(Set4PtFIR, bool);
  vtkGetMacro(Set4PtFIR, bool);

  vtkSetMacro(LatAndElevSmoothingIndex, int);
  vtkGetMacro(LatAndElevSmoothingIndex, int);


protected:
  /*! Constructor */
  vtkPhilips3DProbeVideoSource();
  /*! Destructor */
  virtual ~vtkPhilips3DProbeVideoSource();

  /*! Callback function when a new frame is ready to be added */
  void CallbackAddFrame(vtkImageData* imageData);

  /*! Connect to device */
  virtual PlusStatus InternalConnect();

  /*! Disconnect from device */
  virtual PlusStatus InternalDisconnect();

  /*! InternalUpdate continually checks the connection
   When the iE33 is switched off of 3D mode, the connection doesn't die, it simply stops receiving a callback.
   When the iE33 is switched back, the callbacks don't resume.
   This function checks for long delays between frames and attempts to regain the connection if it detects a timeout.
   */
  virtual PlusStatus InternalUpdate();

  /*! Class for receiving streaming 3D Data */
  vtkIEEListener* Listener;

  /*! The current frame number */
  unsigned long FrameNumber;

  /*! IP Address of the Philips machine*/
  char* IPAddress;

  /*! Port of the Philips machine */
  int Port;

  /*! Parameter to pass to the Philips stream manager */
  bool ForceZQuantize;
  /*! Parameter to pass to the Philips stream manager */
  double ResolutionFactor;
  /*! Parameter to pass to the Philips stream manager */
  bool IntegerZ;
  /*! Parameter to pass to the Philips stream manager */
  bool Isotropic;
  /*! Parameter to pass to the Philips stream manager */
  bool QuantizeDim;
  /*! Parameter to pass to the Philips stream manager */
  int ZDecimation;
  /*! Parameter to pass to the Philips stream manager */
  bool Set4PtFIR;
  /*! Parameter to pass to the Philips stream manager */
  int LatAndElevSmoothingIndex;

private:
  vtkPhilips3DProbeVideoSource(const vtkPhilips3DProbeVideoSource&);  // Not implemented.
  void operator=(const vtkPhilips3DProbeVideoSource&);  // Not implemented.
  static vtkPhilips3DProbeVideoSource* ActiveDevice;
  static bool StreamCallback(_int64 id, SClient3DArray *ed, SClient3DArray *cd);
};

#endif

