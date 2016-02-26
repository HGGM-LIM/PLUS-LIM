/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

/*=========================================================================
The following copyright notice is applicable to parts of this file:
Copyright (c) Siddharth Vikal, Elvis Chen, 2008
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.
Authors include: Siddharth Vikal (Queen's University),
Elvis Chen (Queen's University), Danielle Pace (Robarts Research Institute
and The University of Western Ontario)
=========================================================================*/  

#include "PlusConfigure.h"

// porta includes
#include <porta_params_def.h>
#include <ImagingModes.h>
#include <porta.h>
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION < 6)
  #include <porta_std_includes.h>
#else
  #include <porta_def.h>
  #include <porta_wrapper.h>
  #include "Objbase.h" // required for CoInitialize
#endif

#include "TrackedFrame.h" 
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiThreader.h"
#include "vtkObjectFactory.h"
#include "vtkPlusChannel.h"
#include "vtkPlusDataSource.h"
#include "vtkSonixPortaVideoSource.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"
#include "vtksys/SystemTools.hxx"
#include "vtkMatrix4x4.h"
#include "vtkTransform.h"

#include <ctype.h>

// because of warnings in windows header push and pop the warning level
#ifdef _MSC_VER
#pragma warning (push, 3)
#endif

#include <vector>
#include <string>

#ifdef _MSC_VER
#pragma warning (pop)
#endif

//vtkStandardNewMacro(vtkWin32VideoSource);
//----------------------------------------------------------------------------
// Needed when we don't use the vtkStandardNewMacro.
vtkInstantiatorNewMacro(vtkSonixPortaVideoSource);

//----------------------------------------------------------------------------
vtkSonixPortaVideoSource* vtkSonixPortaVideoSource::Instance = 0;
vtkSonixPortaVideoSourceCleanup vtkSonixPortaVideoSource::Cleanup;

#if ( _MSC_VER >= 1300 ) // Visual studio .NET
#pragma warning ( disable : 4311 )
#pragma warning ( disable : 4312 )
#  define vtkGetWindowLong GetWindowLongPtr
#  define vtkSetWindowLong SetWindowLongPtr
#  define vtkGWL_USERDATA GWLP_USERDATA
#else // regular Visual studio
#  define vtkGetWindowLong GetWindowLong
#  define vtkSetWindowLong SetWindowLong
#  define vtkGWL_USERDATA GWL_USERDATA
#endif //

#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION >= 6) 

  // This is a workaround to avoid linker error due to missing portaImportChromaMap (caused by Ultrasonix SDK 6.1.0 bug)
  int portaImportChromaMap(int index, const unsigned int* lut)
  {
    LOG_ERROR("portaImportChromaMap function is missing from Ultrasonix Porta SDK");
    return 0;
  }

  // The porta wrapper implementation is not included in porta.lib, so we have to include it here
  #include "porta_wrapper.cpp"

#endif

//----------------------------------------------------------------------------
vtkSonixPortaVideoSourceCleanup::vtkSonixPortaVideoSourceCleanup()
{
}

//----------------------------------------------------------------------------
vtkSonixPortaVideoSourceCleanup::~vtkSonixPortaVideoSourceCleanup()
{
  // Destroy any remaining output window.
  vtkSonixPortaVideoSource::SetInstance(NULL);
}

//----------------------------------------------------------------------------
vtkSonixPortaVideoSource::vtkSonixPortaVideoSource()
: DisplayIndex(0)
, AutoClipEnabled(false)
, CurrentDepthMm(-1)
, ImageGeometryChanged(false)
, ImageToTransducerTransformName(NULL)
{
  this->Porta = new porta;
  this->PortaConnected = false;
  this->ProbeInformation = new probeInfo;

  this->Usm = 0;
  this->Pci = 0;
  this->PortaBModeWidth = 480;
  this->PortaBModeHeight = 436;
  this->ImageBuffer = NULL;
  this->ImagingMode = (int)BMode;
  this->PortaProbeName = NULL;
  this->PortaSettingPath = NULL;
  this->PortaLicensePath = NULL;
  this->PortaFirmwarePath = NULL;
  this->PortaLUTPath = NULL;
  this->PortaCineSize = 256 * 1024 * 1024; // defaults to 256MB of Cine
  this->FirstCallToAddFrameToBuffer = true;
  this->VolumeIndex = 0;
  this->FrameIndexInVolume = 0;

  this->FramePerVolume = -1;
  this->StepPerFrame = -1;
  this->MotorRotationRangeDeg = -1;

  this->Zoom = -1;
  this->Depth = -1;
  this->Frequency = -1;
  this->Gain = -1;

  this->LastRotationCcw = false;

  this->RequireImageOrientationInConfiguration = true;
  // No need for StartThreadForInternalUpdates, as we are notified about each new frame through a callback function
}

vtkSonixPortaVideoSource::~vtkSonixPortaVideoSource() 
{
  // clean up porta related sources
  // this->vtkSonixPortaVideoSource::ReleaseSystemResources();

  // release all previously allocated memory
  SetPortaProbeName(NULL);
  SetPortaSettingPath(NULL);
  SetPortaLicensePath(NULL);
  SetPortaFirmwarePath(NULL);
  SetPortaLUTPath(NULL);
  SetImageToTransducerTransformName(NULL);

  delete [] this->ImageBuffer;
  this->ImageBuffer = NULL;

  delete this->ProbeInformation;
  this->ProbeInformation = NULL;

  delete this->Porta;
  this->Porta = NULL;
}

//----------------------------------------------------------------------------
// up the reference count so it behaves like New
vtkSonixPortaVideoSource *vtkSonixPortaVideoSource::New() 
{
  vtkSonixPortaVideoSource *ret = vtkSonixPortaVideoSource::GetInstance();
  ret->Register( NULL );
  return( ret );
}

//----------------------------------------------------------------------------
// Return the single instance of the vtkOutputWindow
vtkSonixPortaVideoSource *vtkSonixPortaVideoSource::GetInstance() 
{

  if ( !vtkSonixPortaVideoSource::Instance ) 
  {
    // try the factory first
    vtkSonixPortaVideoSource::Instance = (vtkSonixPortaVideoSource *)vtkObjectFactory::CreateInstance( "vtkSonixPortaVideoSource" );

    if ( !vtkSonixPortaVideoSource::Instance ) 
    {
      vtkSonixPortaVideoSource::Instance = new vtkSonixPortaVideoSource();
    }

    if ( !vtkSonixPortaVideoSource::Instance ) 
    {
      int error = 0;
    }
  }

  return( vtkSonixPortaVideoSource::Instance );
}

void vtkSonixPortaVideoSource::SetInstance( vtkSonixPortaVideoSource *instance ) 
{
  if ( vtkSonixPortaVideoSource::Instance == instance ) 
  {
    return;
  }

  // preferably this will be NULL
  if ( vtkSonixPortaVideoSource::Instance ) 
  {
    vtkSonixPortaVideoSource::Instance->Delete();
    vtkSonixPortaVideoSource::Instance=NULL;
  }

  vtkSonixPortaVideoSource::Instance = instance;

  if ( !instance ) 
  {
    return;
  }

  //user will call ->Delete() after setting instance
  instance->Register( NULL );
}


//----------------------------------------------------------------------------
std::string vtkSonixPortaVideoSource::GetSdkVersion()
{
  std::ostringstream version; 
  version << "UltrasonixSDK-" << PLUS_ULTRASONIX_SDK_MAJOR_VERSION << "." << PLUS_ULTRASONIX_SDK_MINOR_VERSION << "." << PLUS_ULTRASONIX_SDK_PATCH_VERSION; 
  return version.str(); 
}

//----------------------------------------------------------------------------
void vtkSonixPortaVideoSource::PrintSelf(ostream& os, vtkIndent indent) {
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Imaging mode: " << this->ImagingMode << "\n";
  os << indent << "Frequency: " << this->Frequency << "MHz\n";
}

//----------------------------------------------------------------------------
// the callback function used when there is a new frame of data received
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION < 5) || (PLUS_ULTRASONIX_SDK_MAJOR_VERSION == 5 && PLUS_ULTRASONIX_SDK_MINOR_VERSION < 7)
//  SDK version < 5.7.x 
bool vtkSonixPortaVideoSource::vtkSonixPortaVideoSourceNewFrameCallback( void *param, int id )
#elif (PLUS_ULTRASONIX_SDK_MAJOR_VERSION < 6) 
//  5.7.x <= SDK version < 6.x
bool vtkSonixPortaVideoSource::vtkSonixPortaVideoSourceNewFrameCallback( void *param, int id, int header )
#else
int vtkSonixPortaVideoSource::vtkSonixPortaVideoSourceNewFrameCallback(void* param, int id, int header)
#endif
{

  if ( id == 0 )  
  {
    // no actual data received
    return ( false );
  }

  bool motorRotationCcw = (header & 0x00000002) > 0;
  int motorStepCount = (header & 0xFFFC)>>2; // always starts from 0, no matter what the starting angle was (not an absolute position but from the FOV start position)
  
  vtkSonixPortaVideoSource::GetInstance()->AddFrameToBuffer( param, id, motorRotationCcw, motorStepCount );

  return true;
}

//----------------------------------------------------------------------------
// copy the Device Independent Bitmap from the VFW framebuffer into the
// vtkVideoSource framebuffer (don't do the unpacking yet)
PlusStatus vtkSonixPortaVideoSource::AddFrameToBuffer( void *param, int id, bool motorRotationCcw, int motorStepCount ) 
{
  if (!this->Recording)
  {
    // drop the frame, we are not recording data now
    return PLUS_SUCCESS;
  }

  if( this->OutputChannels.empty() )
  {
    LOG_ERROR("No output channels defined for vtkSonixPortaVideoSource" );
    return PLUS_FAIL;
  }
  vtkPlusChannel* outputChannel=this->OutputChannels[0];

  int frameSize[3] = {0,0,0};
  this->GetInputFrameSize(*outputChannel, frameSize);
  vtkPlusDataSource* aSource(NULL);
  if( outputChannel->GetVideoSource(aSource) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to retrieve the video source in the SonixPorta device.");
    return PLUS_FAIL;
  }
  int frameBufferBytesPerPixel = aSource->GetNumberOfBytesPerPixel(); 
  const int frameSizeInBytes = frameSize[0] * frameSize[1] * frameBufferBytesPerPixel; 

  // for frame containing FC (frame count) in the beginning for data coming from cine, jump 2 bytes
  int numberOfBytesToSkip = 4; 

  // Aligns the motor for correct acqusition of its angle
  if ( this->FirstCallToAddFrameToBuffer )
  {
    this->Porta->setParam( prmMotorStatus, 0 );
    this->Porta->setParam( prmMotorStatus, 1 );
    this->FirstCallToAddFrameToBuffer = false;
    this->LastRotationCcw = !motorRotationCcw; // force initialization of a new volume
    // First volume index will be 0 for CCW rotation, 1 for CW rotation.
    // Even volume index is always CCW, odd index is always CW rotation direction.
    this->VolumeIndex = motorRotationCcw ? -1 : 0;
    this->FrameIndexInVolume = 0;
  }

  if( this->ImageBuffer == NULL )
  {
    LOG_ERROR("vtkSonixPortaVideoSource::AddFrameToBuffer failed: no image buffer is allocated" );
    return PLUS_FAIL;
  }
  this->Porta->getBwImage( 0, this->ImageBuffer, false );

  // get the pointer to the actual incoming data onto a local pointer
  unsigned char *deviceDataPtr = static_cast<unsigned char*>( this->ImageBuffer );

  double motorAngleDeg=0;
  if (this->ProbeInformation->motorized)
  {
    if (motorRotationCcw)
    {
      motorAngleDeg = -this->MotorRotationStartAngleDeg + motorStepCount * this->MotorRotationPerStepDeg;
    }
    else
    {
      motorAngleDeg =  this->MotorRotationStartAngleDeg - motorStepCount * this->MotorRotationPerStepDeg;
    }
    if (this->LastRotationCcw != motorRotationCcw)
    {
      // the motor changed rotation direction
      this->LastRotationCcw = motorRotationCcw;
      this->VolumeIndex++;
      this->FrameIndexInVolume = 0;
    }
  }

  if (this->ImageGeometryChanged)
  {
    this->ImageGeometryChanged = false;
    int currentDepth=-1;
    if (!this->Porta->getParam("b-depth", currentDepth))
    {
      LOG_WARNING("Failed to retrieve b-depth parameter");
    }

    int currentPixelSpacingMicron[2]={-1,-1};
    if (!this->Porta->getMicronsPerPixel(this->DisplayIndex, currentPixelSpacingMicron[0], currentPixelSpacingMicron[1]))
    {
      LOG_WARNING("Failed to retrieve pixel spacing from Porta");
    }

    int line=this->ProbeInformation->elements/2; // middle transducer element
    int sample=0; // transducer surface
    int currentTransducerOriginPixels[2]={-1,-1};
    if (!this->Porta->getPixelCoordinates(this->DisplayIndex, line, sample, currentTransducerOriginPixels[0], currentTransducerOriginPixels[1], false))
    {
      LOG_WARNING("Failed to retrieve transducer origin parameter from Porta");
    }

    this->CurrentDepthMm = currentDepth;

    this->CurrentPixelSpacingMm[0] = 0.001*currentPixelSpacingMicron[0];
    this->CurrentPixelSpacingMm[1] = 0.001*currentPixelSpacingMicron[1];

    int *clipRectangleOrigin = aSource->GetClipRectangleOrigin();
    this->CurrentTransducerOriginPixels[0] = currentTransducerOriginPixels[0]-clipRectangleOrigin[0];
    this->CurrentTransducerOriginPixels[1] = currentTransducerOriginPixels[1]-clipRectangleOrigin[1];
  }

  TrackedFrame::FieldMapType customFields;

  std::ostringstream frameIndexInVolumeStr;
  frameIndexInVolumeStr << this->FrameIndexInVolume;
  customFields["FrameIndex"] = frameIndexInVolumeStr.str();
  std::ostringstream volumeIndexStr;
  volumeIndexStr << this->VolumeIndex;
  customFields["VolumeIndex"] =  volumeIndexStr.str();
  std::ostringstream motorAngleDegStr;
  motorAngleDegStr << motorAngleDeg;
  customFields["MotorAngleDeg"] = motorAngleDegStr.str();
  customFields["MotorToMotorRotatedTransform"] = this->GetMotorToMotorRotatedTransform( motorAngleDeg );
  customFields["MotorToMotorRotatedTransformStatus"] = "OK";
  std::ostringstream motorRotatedToTransducerTransformStr;
  motorRotatedToTransducerTransformStr << "1 0 0 0 0 1 0 " << -this->ProbeInformation->motorRadius*0.001 << " 0 0 1 0 0 0 0 1";
  customFields["MotorRotatedToTransducerTransform"] = motorRotatedToTransducerTransformStr.str();
  customFields["MotorRotatedToTransducerTransformStatus"] = "OK";

  if (this->ImageToTransducerTransformName!=NULL && strlen(this->ImageToTransducerTransformName)>0)
  {
    std::ostringstream imageToTransducerTransformStr;
    double zPixelSpacingMm = (this->CurrentPixelSpacingMm[0]+this->CurrentPixelSpacingMm[1])/2.0; // set to non-zero to keep the matrix as a 3D-3D transformation
    imageToTransducerTransformStr << this->CurrentPixelSpacingMm[0] << " 0 0 " << -1.0*this->CurrentTransducerOriginPixels[0]*this->CurrentPixelSpacingMm[0];
    imageToTransducerTransformStr << " 0 " << this->CurrentPixelSpacingMm[1] << " 0 " << -1.0*this->CurrentTransducerOriginPixels[1]*this->CurrentPixelSpacingMm[1];
    imageToTransducerTransformStr << " 0 0 " << zPixelSpacingMm << " 0";
    imageToTransducerTransformStr << " 0 0 0 1";
    customFields["ImageToTransducerTransform"] = imageToTransducerTransformStr.str();
    customFields["ImageToTransducerTransformStatus"] = "OK";
  }

  PlusStatus status = aSource->AddItem(deviceDataPtr, aSource->GetInputImageOrientation(), frameSize, VTK_UNSIGNED_CHAR, 1, US_IMG_BRIGHTNESS, numberOfBytesToSkip, id, UNDEFINED_TIMESTAMP, UNDEFINED_TIMESTAMP, &customFields); 

  this->Modified();
  return status;
}

//----------------------------------------------------------------------------
std::string vtkSonixPortaVideoSource::GetMotorToMotorRotatedTransform( double angle )
{
  vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();

  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->SetMatrix( matrix );
  transform->RotateX( angle ); 
  transform->GetMatrix(matrix);
  
  std::ostringstream matrixToString;
  matrixToString << 
    matrix->GetElement(0,0) <<" "<< matrix->GetElement(0,1) <<" "<< matrix->GetElement(0,2) <<" "<< matrix->GetElement(0,3) << " " <<
    matrix->GetElement(1,0) <<" "<< matrix->GetElement(1,1) <<" "<< matrix->GetElement(1,2) <<" "<< matrix->GetElement(1,3) << " " <<
    matrix->GetElement(2,0) <<" "<< matrix->GetElement(2,1) <<" "<< matrix->GetElement(2,2) <<" "<< matrix->GetElement(2,3) << " " <<
    matrix->GetElement(3,0) <<" "<< matrix->GetElement(3,1) <<" "<< matrix->GetElement(3,2) <<" "<< matrix->GetElement(3,3) ;

  return matrixToString.str();
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::InternalConnect() 
{
  vtkPlusDataSource* aSource(NULL);
  if( this->GetFirstVideoSource(aSource) != PLUS_SUCCESS )
  {
    LOG_ERROR(this->GetDeviceId() << ": Unable to retrieve video source.");
    return PLUS_FAIL;
  }

  // Set B-mode image size
  delete [] this->ImageBuffer;
  this->ImageBuffer = new unsigned char [ this->PortaBModeWidth * this->PortaBModeHeight * 4 ];
  if (this->ImageBuffer == NULL) 
  {
    LOG_ERROR("vtkSonixPortaVideoSource constructor: not enough memory for ImageBuffer" );
    return PLUS_FAIL;
  }

  LOG_TRACE("InternalConnect start");
  if ( this->PortaSettingPath == 0 ||
    this->PortaFirmwarePath == 0 ||
    this->PortaLicensePath == 0 ||
    this->PortaLUTPath == 0 ) 
  {
    LOG_ERROR("One of the Porta paths has not been set (check PortaSettingPath, PortaFirmwarePath, PortaLicensePath, PortaLUTPath attributes)" );
    return PLUS_FAIL;
  }

#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION >= 6)
  // CoInitialize required. Without that this->Porta->loadPreset fails.
  CoInitialize(NULL);
#endif

  int channels = (this->Usm == 2) ? 32 : 64; // from Ultrasonix SDK PortaDemo.cpp
  
  LOG_TRACE("Porta initialization:"
    << " PortaFirmwarePath="<<this->PortaFirmwarePath
    << " PortaSettingPath="<<this->PortaSettingPath
    << " PortaLicensePath="<<this->PortaLicensePath
    << " PortaLUTPath="<<this->PortaLUTPath
    << " Usm="<<this->Usm
    << " Pci="<<this->Pci
    << " channels="<<channels);
  if ( !this->Porta->init( this->PortaCineSize,
    this->PortaFirmwarePath,
    this->PortaSettingPath,
    this->PortaLicensePath,
    this->PortaLUTPath,
    this->Usm, 
    this->Pci,
    0, // high voltage (from Ultrasonix SDK PortaDemo.cpp)
    1, // DDR (from Ultrasonix SDK PortaDemo.cpp)
    channels) )
  {
    LOG_ERROR("Initialize: Porta could not be initialized: (" << this->GetLastPortaError() << ")"
		<< " PortaFirmwarePath="<<this->PortaFirmwarePath
		<< " PortaSettingPath="<<this->PortaSettingPath
		<< " PortaLicensePath="<<this->PortaLicensePath
		<< " PortaLUTPath="<<this->PortaLUTPath
		<< " Usm="<<this->Usm
		<< " Pci="<<this->Pci
		<< " channels="<<channels);
    this->PortaConnected = false;
    return PLUS_FAIL;
  }

  this->PortaConnected = true;
  this->FirstCallToAddFrameToBuffer = true;

  this->ImageGeometryChanged = true; // trigger an initial update of geometry info

  // select the probe
  
  if ( !this->Porta->isConnected() )
  {
    LOG_ERROR("Porta is not connected");
    this->InternalDisconnect();
    return PLUS_FAIL;
  }
  
  // the 3D/4D probe is always connected to port 0
  const int connectorId = 0;
  int probeId = (char)this->Porta->getProbeID( connectorId );
  if ( !this->Porta->selectProbe( probeId ) )
  {
    LOG_ERROR("Porta connect error: failed to select probe plugged into connector "<<connectorId);
    this->InternalDisconnect();
    return PLUS_FAIL;
  }

  // select the probeId read and see if it is motorized
  if (!this->Porta->getProbeInfo( *this->ProbeInformation ))
  {
    LOG_ERROR("Porta connect error: failed to get probe information");
    this->InternalDisconnect();
    return PLUS_FAIL;
  }

  const int MAX_NAME_LENGTH=100;
  char name[MAX_NAME_LENGTH+1];
  name[MAX_NAME_LENGTH]=0;
  this->Porta->activateProbeConnector( connectorId );
  this->Porta->getProbeName( name, MAX_NAME_LENGTH, probeId );  
  LOG_DEBUG("Probe connected to connector "<<probeId<<": "<<name);

  // store the probe name
  SetPortaProbeName(name);
  if ( !this->Porta->findMasterPreset( name, MAX_NAME_LENGTH, probeId ) ) 
  {
    LOG_ERROR("Porta connect error: master preset cannot be found" );
    this->InternalDisconnect();
    return PLUS_FAIL;
  }

  if ( !this->Porta->loadPreset( name ) )
  {
    LOG_ERROR("Porta connect error: master preset could not be loaded" );
    this->InternalDisconnect();
    return PLUS_FAIL;
  }

  if( !this->SetInputFrameSize( *aSource, this->PortaBModeWidth, this->PortaBModeHeight, 1 )  )
  {
    LOG_ERROR("Initializer: can not set the frame size" );
  }

  // It is necessary to enclose the portaInitMode and custom display initialization by 
  // a freeze-unfreeze for proper operation. This is because if the imaging was
  // running prior to the call to portaInitMode, the function will try to restart
  // imaging at the end of the call, but the custom displays are not initialized
  // yet - Ali B, Apr 2014
  int isRunning = this->Porta->isImaging();
  if (isRunning)
  {
      this->Porta->stopImage();
  }

  // this is from propello
  if( !this->Porta->initImagingMode( BMode ) ) 
  {    
    LOG_ERROR("Initialize: cannot initialize imagingMode (" << this->GetLastPortaError() << ")" );
    return PLUS_FAIL;
  }

  this->Porta->setDisplayDimensions( this->DisplayIndex, this->PortaBModeWidth, this->PortaBModeHeight );
  
  // successfully set to bmode

  // Set up imaging parameters
  // Parameter value <0 means that the parameter should be kept unchanged
  if (this->Frequency >= 0) { SetFrequency(this->Frequency); }
  if (this->Depth >= 0) { SetDepth(this->Depth); }
  if (this->Gain >= 0 ) { SetGain(this->Gain); }
  if (this->Zoom >= 0 ) { SetZoom(this->Zoom); }

  this->MotorRotationPerStepDeg = 0;
  this->MotorRotationStartAngleDeg = 0;
  if(this->ProbeInformation->motorized ) 
  {
    // Compute the angle per step
    if (this->ProbeInformation->motorSteps>0)
    {
      this->MotorRotationPerStepDeg = static_cast<double>(this->ProbeInformation->motorFov) * 0.001 / static_cast<double>(this->ProbeInformation->motorSteps);
    }

    if (this->MotorRotationRangeDeg >= 0) { SetMotorRotationRangeDeg(this->MotorRotationRangeDeg); }
    if (this->StepPerFrame >= 0) { SetStepPerFrame(this->StepPerFrame); }
    if (this->FramePerVolume >= 0) { SetFramePerVolume(this->FramePerVolume); }

    {
    int actualFramePerVolume=0;
    GetFramePerVolume(actualFramePerVolume);
    int actualStepPerFrame = 0;
    GetStepPerFrame(actualStepPerFrame);
    double actualMotorRotationRangeDeg = 0;
    GetMotorRotationRangeDeg(actualMotorRotationRangeDeg);    
    this->MotorRotationStartAngleDeg = (actualFramePerVolume-1)/2*actualStepPerFrame*this->MotorRotationPerStepDeg;
    LOG_INFO("actual values in hw: FramePerVolume="<<actualFramePerVolume<<"    StepPerFrame="<<actualStepPerFrame<<"    MotorRotationRangeDeg="<<actualMotorRotationRangeDeg<<"    MotorRotationPerStepDeg="<<this->MotorRotationPerStepDeg<<"   MotorRotationStartAngleDeg="<<this->MotorRotationStartAngleDeg);
    }

    // Turn on the motor
    this->Porta->setParam( prmMotorStatus, 1 );
  }
  else
  {
    if (this->FramePerVolume >= 0 || this->StepPerFrame >= 0 || this->MotorRotationRangeDeg >= 0)
    {
      LOG_WARNING("Porta motor parameters are specified but the currently connected probe is not motorized");
    }
  }

  // finally, update all the parameters
  if ( !this->UpdateSonixPortaParams() ) 
  {
    LOG_ERROR("Initialize: cannot update sonix params" ); 
  }

  // Set up the callback function which is invocked upon arrival of a new frame
  this->Porta->setDisplayCallback( 0, vtkSonixPortaVideoSourceNewFrameCallback,(void*)this ); 

  LOG_DEBUG("Successfully connected to sonix porta video device");
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::InternalDisconnect()
{
  this->Porta->setParam( prmMotorStatus, 0 );
  this->Porta->stopImage();
  this->PortaConnected = false;
  this->Porta->shutdown();
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION >= 6)
  CoUninitialize();
#endif
  return PLUS_SUCCESS;
}
//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::InternalStartRecording() 
{
  if ( !this->Porta->isImaging() )
  {
    this->Porta->runImage();
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::InternalStopRecording() 
{
  if ( this->Porta->isImaging() )
  {
    this->Porta->stopImage();
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::ReadConfiguration(vtkXMLDataElement* rootConfigElement)
{
  LOG_TRACE("vtkSonixPortaVideoSource::ReadConfiguration");
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);

  XML_READ_ENUM1_ATTRIBUTE_OPTIONAL(ImagingMode, deviceConfig, "BMode", BMode)

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, Depth, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, Gain, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, Zoom, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, Frequency, deviceConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, PortaBModeWidth, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, PortaBModeHeight, deviceConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, StepPerFrame, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, FramePerVolume, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, MotorRotationRangeDeg, deviceConfig);

  XML_READ_SCALAR_ATTRIBUTE_REQUIRED(int, Usm, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_REQUIRED(int, Pci, deviceConfig);

  XML_READ_STRING_ATTRIBUTE_REQUIRED(PortaLUTPath, deviceConfig);
  XML_READ_STRING_ATTRIBUTE_REQUIRED(PortaSettingPath, deviceConfig);
  XML_READ_STRING_ATTRIBUTE_REQUIRED(PortaLicensePath, deviceConfig);
  XML_READ_STRING_ATTRIBUTE_REQUIRED(PortaFirmwarePath, deviceConfig);

  XML_READ_BOOL_ATTRIBUTE_OPTIONAL(AutoClipEnabled, deviceConfig);
  XML_READ_STRING_ATTRIBUTE_OPTIONAL(ImageToTransducerTransformName, deviceConfig);

  double obsolete = 0;
  if ( deviceConfig->GetScalarAttribute("HighVoltage", obsolete)) 
  {
    LOG_WARNING("SonixPortaVideo HighVoltage attribute is ignored (HighVoltage is always disabled)");
  }
  if ( deviceConfig->GetScalarAttribute("Channels", obsolete)) 
  {
    LOG_WARNING("SonixPortaVideo Channels attribute is ignored (number of channels is now automatically determined from USM (ultrasound module version) attribute.");
  }

  LOG_DEBUG("Porta read the XML configuration");
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::WriteConfiguration(vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);

  if (this->ImagingMode == BMode)
  {
    deviceConfig->SetAttribute("ImagingMode", "BMode");
  }
  else
  {
    LOG_ERROR("Writing of unsupported ImagingMode requested");
  }

  deviceConfig->SetIntAttribute("Depth", this->Depth);
  deviceConfig->SetIntAttribute("Gain", this->Gain);
  deviceConfig->SetIntAttribute("Zoom", this->Zoom);
  deviceConfig->SetIntAttribute("Frequency", this->Frequency);
  deviceConfig->SetIntAttribute("StepPerFrame", this->StepPerFrame);
  // only save frame per volume or rotation range if specified (depends on SDK version
  // which one is actually used)
  if (this->FramePerVolume>=0)
  {
    deviceConfig->SetIntAttribute("FramePerVolume", this->FramePerVolume);
  }
  if (this->MotorRotationRangeDeg>=0)
  {
    deviceConfig->SetDoubleAttribute("MotorRotationRangeDeg", this->MotorRotationRangeDeg);
  }
  deviceConfig->SetIntAttribute("USM", this->Usm);
  deviceConfig->SetIntAttribute("PCI", this->Pci);
  deviceConfig->SetAttribute("PortaLUTPath", this->PortaLUTPath);
  deviceConfig->SetAttribute("PortaSettingPath", this->PortaSettingPath);
  deviceConfig->SetAttribute("PortaLicensePath", this->PortaLicensePath);
  deviceConfig->SetAttribute("PortaFirmwarePath", this->PortaFirmwarePath);

  XML_WRITE_BOOL_ATTRIBUTE(AutoClipEnabled, deviceConfig);
  XML_WRITE_STRING_ATTRIBUTE_REMOVE_IF_NULL(ImageToTransducerTransformName, deviceConfig);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
std::string vtkSonixPortaVideoSource::GetLastPortaError()
{
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION < 6)
  const unsigned int MAX_PORTA_ERROR_MSG_LENGTH=256;
  char err[MAX_PORTA_ERROR_MSG_LENGTH+1];
  err[MAX_PORTA_ERROR_MSG_LENGTH]=0; // make sure the string is null-terminated
  this->Porta->getLastError(err,MAX_PORTA_ERROR_MSG_LENGTH);
  return err; 
#else
  return "unknown";
#endif
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetParamValue(char* paramId, int paramValue, int &validatedParamValue)
{
  if (!this->PortaConnected)
  {
    // Connection has not been established yet. Parameter value will be set upon connection.
    validatedParamValue=paramValue;
    return PLUS_SUCCESS;
  }
  if (!this->Porta->setParam(paramId, paramValue))
  {
    LOG_ERROR("vtkSonixPortaVideoSource::SetParamValue failed (paramId="<<paramId<<", paramValue="<<paramValue<<") "<<GetLastPortaError());
    return PLUS_FAIL;
  }
  validatedParamValue=paramValue;
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetParamValue(char* paramId, int& paramValue, int &validatedParamValue)
{
  if (!this->PortaConnected)
  {
    // Connection has not been established yet. Returned the cached value.
    paramValue=validatedParamValue;
    return PLUS_SUCCESS;
  }
  paramValue=-1;
  if (!this->Porta->getParam(paramId, paramValue))
  {
    LOG_ERROR("vtkSonixPortaVideoSource::GetParamValue failed (paramId="<<paramId<<", paramValue="<<paramValue<<") "<<GetLastPortaError());
    return PLUS_FAIL;
  }
  return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetFrequency(int aFrequency)
{
  return SetParamValue(prmBTxFreq, aFrequency, this->Frequency);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetFrequency(int& aFrequency)
{
  return GetParamValue(prmBTxFreq, aFrequency, this->Frequency);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetDepth(int aDepth)
{
  return SetParamValue(prmBImageDepth, aDepth, this->Depth);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetDepth(int& aDepth)
{
  return GetParamValue(prmBImageDepth, aDepth, this->Depth);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetGain(int aGain)
{
  return SetParamValue(prmBGain, aGain, this->Gain);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetGain(int& aGain)
{
  return GetParamValue(prmBGain, aGain, this->Gain);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetZoom(int aZoom)
{
  return SetParamValue(prmZoom, aZoom, this->Zoom);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetZoom(int& aZoom)
{
  return GetParamValue(prmZoom, aZoom, this->Zoom);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetFramePerVolume(int aFramePerVolume)
{
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION >= 6)
  LOG_WARNING("FramePerVolume parameter cannot be set in Ultrasonix SDK-6.x. Use MotorRotationRangeDeg parameter instead.");
#endif
  return SetParamValue(prmMotorFrames, aFramePerVolume, this->FramePerVolume);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetFramePerVolume(int& aFramePerVolume)
{
  return GetParamValue(prmMotorFrames, aFramePerVolume, this->FramePerVolume);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetMotorRotationRangeDeg(double aMotorRotationRangeDeg)
{
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION < 6)
  LOG_WARNING("MotorRotationRangeDeg parameter is not available in Ultrasonix SDK-6.x. Use FramePerVolume parameter instead.");
  return PLUS_FAIL;
#else
  int aMotorRotationRangeMilliDeg = int(aMotorRotationRangeDeg*1000);
  int aMotorRotationRangeMilliDegStored = int(this->MotorRotationRangeDeg*1000);
  if (SetParamValue(prmMotorFOV, aMotorRotationRangeMilliDeg, aMotorRotationRangeMilliDegStored)==PLUS_FAIL)
  {
    return PLUS_FAIL;
  }
  this->MotorRotationRangeDeg = aMotorRotationRangeMilliDegStored * 0.001;
  return PLUS_SUCCESS;
#endif
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetMotorRotationRangeDeg(double& aMotorRotationRangeDeg)
{
#if (PLUS_ULTRASONIX_SDK_MAJOR_VERSION < 6)
  LOG_WARNING("MotorRotationRangeDeg parameter is not available in Ultrasonix SDK-6.x. Use FramePerVolume parameter instead.");
  return PLUS_FAIL;
#else
  int aMotorRotationRangeMilliDeg = int(aMotorRotationRangeDeg*1000);
  int aMotorRotationRangeMilliDegStored = int(this->MotorRotationRangeDeg*1000);
  if (GetParamValue(prmMotorFOV, aMotorRotationRangeMilliDeg, aMotorRotationRangeMilliDegStored)==PLUS_FAIL)
  {
    return PLUS_FAIL;
  }
  aMotorRotationRangeDeg = aMotorRotationRangeMilliDegStored * 0.001;
  return PLUS_SUCCESS;
#endif
}


//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::SetStepPerFrame(int aStepPerFrame)
{
  return SetParamValue(prmMotorSteps, aStepPerFrame, this->StepPerFrame);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::GetStepPerFrame(int& aStepPerFrame)
{
  return GetParamValue(prmMotorSteps, aStepPerFrame, this->StepPerFrame);
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::UpdateSonixPortaParams() 
{
  bool bRunning = this->Porta->isImaging();

  if ( bRunning ) 
  {
    this->Porta->stopImage();
  }

  // update VTK FrameRate with SonixRP's hardware frame rate
  //
  // The reasons we update it here is because the SonixRP's hardware
  // frame rate is a function of several parameters, such as
  // bline density and image-depths.
  //
  this->AcquisitionRate = (float)(this->Porta->getFrameRate() );

  if ( bRunning ) 
  {
    this->Porta->runImage();
  }

  this->Modified();

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkSonixPortaVideoSource::NotifyConfigured()
{
  if( this->OutputChannels.size() > 1 )
  {
    LOG_WARNING("vtkSonixPortaVideoSource is expecting one output channel and there are " << this->OutputChannels.size() << " channels. First output channel will be used.");
    return PLUS_FAIL;
  }

  if( this->OutputChannels.empty() )
  {
    LOG_ERROR("No output channels defined for vtkSonixPortaVideoSource. Cannot proceed." );
    this->SetCorrectlyConfigured(false);
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}
