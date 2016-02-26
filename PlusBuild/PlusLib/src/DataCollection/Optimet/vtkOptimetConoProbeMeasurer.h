/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

/*========================================================================
Date: Ag 2015
Authors include:
- Mikael Brudfors [*] brudfors@hggm.es
- Laura Sanz [*] lsanz@hggm.es
- Veronica Garcia [*][�] vgarcia@hggm.es
- Javier Pascau [*][�] jpascau@hggm.es
[*] Laboratorio de Imagen Medica, Hospital Gregorio Maranon - http://image.hggm.es/
[�] Departamento de Bioingenier�a e Ingenier�a Aeroespacial. Universidad Carlos III de Madrid
==========================================================================*/

#ifndef __vtkOptimetConoProbeMeasurer_h
#define __vtkOptimetConoProbeMeasurer_h

#include "vtkDataCollectionExport.h"

#include "vtkPlusDevice.h"

/*!
\class vtkOptimetConoProbeMeasurer 
\brief Interface for the Optimet ConoProbe
This class talks with Optimet ConoProbe over the Optimet Smart32 SDK
Requires PLUS_USE_OPTIMET_CONOPROBE option in CMake.
\ingroup PlusLibDataCollection
*/

class vtkMultiThreader;
class ISmart;

class vtkDataCollectionExport vtkOptimetConoProbeMeasurer : public vtkPlusDevice
{
public:

  static vtkOptimetConoProbeMeasurer *New();
  vtkTypeMacro( vtkOptimetConoProbeMeasurer,vtkPlusDevice );
  void PrintSelf( ostream& os, vtkIndent indent );

  virtual bool IsTracker() const { return true; }

  /*! Connect to device */
  PlusStatus InternalConnect();

  /*! Disconnect from device */
  virtual PlusStatus InternalDisconnect();

  /*! Read configuration from xml data */
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* config); 

  /*! Write configuration to xml data */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* config);

  /*! ... */
  PlusStatus Start();

  /*! ... */
  PlusStatus Stop();

  vtkSetMacro(LaserPower, UINT16);
  vtkSetMacro(Frequency, UINT16);

protected:

  vtkOptimetConoProbeMeasurer();
  ~vtkOptimetConoProbeMeasurer();

  /*! Stop the tracking system and bring it back to its ground state: Initialized, not tracking, at 9600 Baud. */
  PlusStatus InternalUpdate();

private:  // Functions.
  vtkOptimetConoProbeMeasurer( const vtkOptimetConoProbeMeasurer& );
  void operator=( const vtkOptimetConoProbeMeasurer& );

  /*! Get composite laser power. */
  unsigned short CalculateCompositeLaserPower(UINT16 coarseLaserPower, UINT16 fineLaserPower);

  /*! Opens the Probe Dialog. */
  static void* ProbeDialogThread(void* ptr);

private:  // Variables.

  vtkPlusDataSource* MeasurementTool;	

  /*! ConoProbe device handle. */
  ISmart* ConoProbe;

  /*! Defines the position of the lens origin w.r.t. a tracking tool */
  double LensOriginAlignment[6];

  /*! Delay between measurements. */
  UINT16 DelayBetweenMeasurements;

  /*! Frequency. */
  UINT16 Frequency;

  /*! Laser Power. */
  UINT16 LaserPower;

  /*! Coarse laser power. */
  unsigned short CoarseLaserPower;

  /*! Fine laser power. */
  unsigned short FineLaserPower;

  /*! ... */
  vtkMultiThreader* Thread;

  /*! ... */
  int ThreadID;

  /*! ... */
  bool ProbeDialogOpen;
};

#endif