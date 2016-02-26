/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "vtkImageData.h"
#include "vtkIntersonVideoSource.h"
#include "vtkObjectFactory.h"
#include "vtkPlusChannel.h"
#include "vtkPlusDataSource.h"
#include "vtkUSImagingParameters.h"

// Order matters, leave these at the bottom
#include "BmodeDLL.h"
#include "usbProbeDLL_net.h"

// Interson's OEM ID (probes released by Interson have this OEM ID)
#define OEM_ID_INTERSON 0x00
#define TOLERANCE 0.001

vtkStandardNewMacro(vtkIntersonVideoSource);

class vtkIntersonVideoSource::vtkInternal
{
public:
  vtkIntersonVideoSource *External;
  vtkUsImagingParameters* ImagingParameters;

  // TODO : move any of these into imaging parameters?
  bool Interpolate;
  bool BidirectionalScan;
  bool Frozen;

  int ClockDivider;
  double ClockFrequencyMHz;
  int PulseFrequencyDivider;

  double LutCenter;
  double LutWindow;
  int ImageSize[2];
  double PulseVoltage;

  // ProbeButtonPressCount is incremented each time the button on the probe is pressed
  // The value is available in the output channel in the translation component of the ProbeButtonToDummyTransform
  int ProbeButtonPressCount;
  bool EnableProbeButtonMonitoring;

  HWND ImageWindowHandle;
  HBITMAP DataHandle;
  HANDLE ProbeHandle;    
  std::vector<unsigned char> MemoryBitmapBuffer;
  BITMAP Bitmap;
  bmBITMAPINFO	BitmapInfo;
  BYTE *RfDataBuffer;
  static const int samplesPerLine = 2048;

  //----------------------------------------------------------------------------
  vtkIntersonVideoSource::vtkInternal::vtkInternal(vtkIntersonVideoSource* external) 
    : External(external)
    , RfDataBuffer(NULL)
    , ProbeHandle(NULL)
    , ImagingParameters(vtkUsImagingParameters::New())
  {

  }

  //----------------------------------------------------------------------------
  virtual vtkIntersonVideoSource::vtkInternal::~vtkInternal() 
  {
    ImagingParameters->Delete();
    this->External = NULL;
  }

  //----------------------------------------------------------------------------
  void vtkIntersonVideoSource::vtkInternal::PrintSelf(ostream& os, vtkIndent indent) 
  {
    this->ImagingParameters->PrintSelf(os, indent);

    os << indent << "Interpolate: " << this->Interpolate << std::endl;
    os << indent << "BidirectionalScan: " << this->BidirectionalScan << std::endl;
    os << indent << "Frozen: " << this->Frozen << std::endl;
    os << indent << "ClockDivider: " << this->ClockDivider << std::endl;
    os << indent << "ClockFrequencyMHz: " << this->ClockFrequencyMHz << std::endl;
    os << indent << "PulseFrequencyDivider: " << this->PulseFrequencyDivider << std::endl;
    os << indent << "LutCenter: " << this->LutCenter << std::endl;
    os << indent << "LutWindow: " << this->LutWindow << std::endl;
    os << indent << "ImageSize: " << this->ImageSize[0] << ", " << this->ImageSize[1] << std::endl;
    os << indent << "PulseVoltage: " << this->PulseVoltage << std::endl;
    os << indent << "ProbeButtonPressCount: " << this->ProbeButtonPressCount << std::endl;
    os << indent << "EnableProbeButtonMonitoring: " << this->EnableProbeButtonMonitoring << std::endl;
  }  

  //----------------------------------------------------------------------------
  void vtkIntersonVideoSource::vtkInternal::CreateLinearTGC(int tgcMin, int tgcMax)
  {
    int tgc[samplesPerLine]={0};
    int b = tgcMin; 
    float m = (float) (tgcMax - tgcMin) / samplesPerLine;
    for (int x = 0; x < samplesPerLine; x++)
    {
      tgc[x] = (int) (m * (float) x) + b;
    }
    bmSetTGC(tgc);
  }

  //----------------------------------------------------------------------------
  void vtkIntersonVideoSource::vtkInternal::CreateLinearTGC(int initialTGC, int midTGC, int farTGC)
  {
    /*  A linear TGC function is created. The initial point is initialTGC, then is linear until the
    middle point (midTGC) and then linear until the maximum depth where the compensation is equal to farTGC*/

    int tgc[samplesPerLine]={0};
    double firstSlope = (double) (midTGC-initialTGC ) / (samplesPerLine/2);
    double secondSlope = (double) (farTGC-midTGC ) / (samplesPerLine/2);
    for (int x = 0; x < samplesPerLine/2; x++)
    {
      tgc[x] = (int) (firstSlope * (double) x) + initialTGC;
      tgc[samplesPerLine/2 +x] = (int) (secondSlope * (double) x) + midTGC;
    }
    bmSetTGC(tgc);
  }

  //----------------------------------------------------------------------------
  static LRESULT CALLBACK vtkIntersonVideoSource::vtkInternal::ImageWindowProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
  {
    vtkIntersonVideoSource::vtkInternal* self = (vtkIntersonVideoSource::vtkInternal*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    return DefWindowProc (hwnd, iMsg, wParam, lParam) ;
  }

  //----------------------------------------------------------------------------
  PlusStatus vtkIntersonVideoSource::vtkInternal::InitializeDIB(int imageSize[2])
  {
    this->BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    this->BitmapInfo.bmiHeader.biWidth = imageSize[0];
    this->BitmapInfo.bmiHeader.biHeight = -imageSize[1];
    this->BitmapInfo.bmiHeader.biPlanes = 1;
    this->BitmapInfo.bmiHeader.biBitCount = 8;
    this->BitmapInfo.bmiHeader.biCompression = 0;
    this->BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    this->BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    this->BitmapInfo.bmiHeader.biClrUsed = 0;
    this->BitmapInfo.bmiHeader.biClrImportant = 0;

    // Compute the number of bytes in the array of color  
    // indices and store the result in biSizeImage.  
    // The width must be DWORD aligned unless the bitmap is RLE compressed. 
    this->BitmapInfo.bmiHeader.biSizeImage = ((imageSize[0]*8+31)&~31)/8 * imageSize[1]; 

    for( int i=0; i<256; i++ )
    {
      this->BitmapInfo.bmiColors[i].rgbRed = i;
      this->BitmapInfo.bmiColors[i].rgbBlue = i;
      this->BitmapInfo.bmiColors[i].rgbGreen = i;
      this->BitmapInfo.bmiColors[i].rgbReserved = 0;
    }

    return PLUS_SUCCESS;
  }

  //----------------------------------------------------------------------------
  void CreateLinearLUT(BYTE lut[], int level, int window)
  {
    int center = window / 2;				// center of window
    int left = level - center;				// left of window
    int right = level + center;				// right of window

    // everything to our left is black
    for (int x=0; x<left; x++)
    {
      lut[x] = 0;		
    }

    // everything to our right is white
    for (int x=right+1; x < 256; x++)
    {
      lut[x] = 255;
    }

    // everything in between is on the line
    float m = 255.0f / ((float) window);

    int startX=left;
    if (startX<0)
    {
      startX=0;
    }
    int endX=right;
    if (endX>255)
    {
      endX=255;
    }
    for (int x=startX; x <= endX; x++) 
    {
      int y = (int) (m * (float)(x-left) + 0.5f);
      if (y<0)
      {
        y=0;
      }
      else if (y>255)
      {
        y=255;
      }
      lut[x] = y;
    }
  }

  //----------------------------------------------------------------------------
  void CreateLUT(BYTE lut[])
  {
    int center = this->LutWindow / 2;				// center of window
    int left = this->LutCenter - center;				// left of window
    int right = this->LutCenter + center;				// right of window
    double contrast;
    this->ImagingParameters->GetContrast(contrast);
    double brightness;
    this->ImagingParameters->GetIntensity(brightness);
    for (int x=0; x <= 255; x++) 
    {
      int y = (int) ((float)contrast/256.0f * (float)(x-128) + brightness);
      if (y<left)
      {
        y=0;
      }
      else if (y>right)
      {
        y=255;
      }
      lut[x] = y;
    }
  }
};

//----------------------------------------------------------------------------
vtkIntersonVideoSource::vtkIntersonVideoSource()
  : Internal(new vtkInternal(this))
{
  this->Internal->Interpolate = true;
  this->Internal->BidirectionalScan = true;
  this->Internal->Frozen = true;

  this->RequireImageOrientationInConfiguration = true;

  this->Internal->ClockDivider = 1;
  this->Internal->PulseFrequencyDivider = 2;

  this->Internal->ImagingParameters->SetIntensity(128.0); //192
  this->Internal->ImagingParameters->SetContrast(256.0); //128
  this->Internal->LutCenter = 128; //192
  this->Internal->LutWindow = 256; //128

  this->Internal->ImageSize[0]=800;
  this->Internal->ImageSize[1]=512;

  this->Internal->PulseVoltage=30.0f;

  // No callback function provided by the device, so the data capture thread will be used to poll the hardware and add new items to the buffer
  this->StartThreadForInternalUpdates=true;
  this->AcquisitionRate = 30;

  this->Internal->EnableProbeButtonMonitoring = false;
  this->Internal->ProbeButtonPressCount = 0;
}

//----------------------------------------------------------------------------
vtkIntersonVideoSource::~vtkIntersonVideoSource()
{ 
  if( !this->Connected )
  {
    this->Disconnect();
  }

  delete this->Internal;
  this->Internal=NULL;
}

//----------------------------------------------------------------------------
void vtkIntersonVideoSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  this->Internal->PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
int CALLBACK ProbeAttached()
{
  LOG_INFO("Probe attached");
  return 0;
}

//----------------------------------------------------------------------------
int CALLBACK ProbeDetached()
{
  LOG_INFO("Probe detached");
  return 0;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::InternalConnect()
{
  LOG_TRACE( "vtkIntersonVideoSource::InternalConnect" );

  LOG_DEBUG("Interson Bmode DLL version "<<bmDLLVer()<<", USB probe DLL version "<<usbDLLVer());

  usbSetProbeAttachCallback(&ProbeAttached);
  usbSetProbeDetachCallback(&ProbeDetached);

  // Before any probe can be initialized with usbInitializeProbes, they must be detected. usbFindProbes() 
  // will detect all attached probes and initialize the driver. After a successful call to usbFindProbes, 
  // other probe-related functions may be called. These include: usbInitializeProbes, usbProbeHandle, 
  // usbSelectProbe.
  usbErrorString errorStatus={0};
  ULONG status = usbFindProbes(errorStatus);
  LOG_DEBUG("Find USB probes: status="<<status<<", details: "<<errorStatus);
  if (status != ERROR_SUCCESS)
  {
    LOG_ERROR("Interson finding probes failed");
    return PLUS_FAIL;
  }

  usbInitializeProbes();

  // Turn on USB data synchronization checking
  usbTurnOnSync();

  int numberOfAttachedProbes=usbNumberAttachedProbes();
  LOG_DEBUG("Number of attached probes: "<<numberOfAttachedProbes);
  if (numberOfAttachedProbes==0)
  {
    LOG_ERROR("No Interson probes are attached");
    return PLUS_FAIL;
  }
  if (numberOfAttachedProbes>1)
  {
    LOG_WARNING("Multiple Interson probes are attached, using the first one");
  }


  PVOID display = bmInitializeDisplay(this->Internal->ImageSize[0]*this->Internal->ImageSize[1]);
  if (display == NULL)
  {
    LOG_ERROR("Could not initialize the display");
    return PLUS_FAIL;
  }

  this->Internal->InitializeDIB(this->Internal->ImageSize);

  BYTE currentOemId=usbProbeOEMID();
  if (currentOemId != OEM_ID_INTERSON)
  {
    LOG_WARNING("Probe OEM ID mismatch: "<<OEM_ID_INTERSON<<" (Interson) is expected, got "<<currentOemId);
  }

  // get the first probe
  usbProbeHandle(0, &this->Internal->ProbeHandle); 
  // if there is hardware attached, this enables it
  usbSelectProbe(this->Internal->ProbeHandle);
  // set the display window depth for this probe
  usbSetWindowDepth(this->Internal->ProbeHandle, this->Internal->ImageSize[1]);
  // set the assumed velocity (m/s)
  float soundVelocity = -1 ;
  this->Internal->ImagingParameters->GetSoundVelocity(soundVelocity);
  if (soundVelocity>0)
  {
    usbSetVelocity(this->Internal->ProbeHandle, soundVelocity );
  }
  double depth = -1;
  this->Internal->ImagingParameters->GetDepthMm(depth);
  if (depth>0)
  {
    this->SetDepthMm(depth);
  }

  // Setup the display offsets now that we have the probe and DISPLAY data
  bmSetDisplayOffset(0);

  if (usbSetCineBuffers(32) != 32)
  {
    LOG_ERROR("Could not allocate Cine buffers.");
    return PLUS_FAIL;
  }	
  this->Internal->RfDataBuffer = usbCurrentCineFrame();  

  usbSetUnidirectionalMode();
  usbSetPulseVoltage(this->Internal->PulseVoltage);

  POINT ptCenter;		// Points for Zoomed Display
  ptCenter.x = this->Internal->ImageSize[0]/2;
  ptCenter.y = this->Internal->ImageSize[1]/2;
  int rotation=0;

  if (bmCalculateDisplay (this->Internal->ImageSize[0], this->Internal->ImageSize[1], ptCenter, this->Internal->ProbeHandle, this->Internal->ImageSize[0], rotation) == ERROR)
  {
    LOG_ERROR("CalculateDisplay failed");
  }

  std::string probeName;
  GetProbeNameDevice(probeName);
  LOG_DEBUG("Interson probe name: "<<probeName<<", ID: "<<usbProbeID(this->Internal->ProbeHandle));

  vtkPlusDataSource* aSource=NULL;
  if( this->GetFirstActiveOutputVideoSource(aSource) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to retrieve the video source in the IntersonVideo device.");
    return PLUS_FAIL;
  }

  // Clear buffer on connect because the new frames that we will acquire might have a different size 
  aSource->Clear();
  aSource->SetPixelType( VTK_UNSIGNED_CHAR );  
  aSource->SetInputFrameSize(this->Internal->ImageSize[0], this->Internal->ImageSize[1], 1); 

  HINSTANCE hInst = GetModuleHandle(NULL);

  WNDCLASSEX		wndclass;
  wndclass.cbSize        = sizeof (wndclass);
  wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
  wndclass.lpfnWndProc   = vtkIntersonVideoSource::vtkInternal::ImageWindowProc;
  wndclass.cbClsExtra    = 0;
  wndclass.cbWndExtra    = 0;
  wndclass.hInstance     = hInst;
  wndclass.hIcon         = NULL;
  wndclass.hCursor       = NULL;
  wndclass.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
  wndclass.lpszMenuName  = NULL ;
  wndclass.lpszClassName = TEXT("ImageWindow");
  wndclass.hIconSm       = NULL;
  RegisterClassEx(&wndclass);

  this->Internal->ImageWindowHandle = CreateWindow( TEXT("ImageWindow"), TEXT("Ultrasound"), 
    WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0,
    this->Internal->ImageSize[0]+2*GetSystemMetrics(SM_CXFIXEDFRAME),
    this->Internal->ImageSize[1]+2*GetSystemMetrics(SM_CYFIXEDFRAME)+GetSystemMetrics(SM_CYBORDER)+GetSystemMetrics(SM_CYSIZE),
    NULL, NULL, hInst, NULL);  

  if (this->Internal->ImageWindowHandle==NULL)
  {
    LOG_ERROR("Failed to create capture window");
    return PLUS_FAIL;
  }

  SetWindowLongPtr(this->Internal->ImageWindowHandle, GWLP_USERDATA, (LONG)this->Internal);

  // Create a bitmap for use in our DIB
  HDC hdc = GetDC(this->Internal->ImageWindowHandle) ;
  RECT rect;
  GetClientRect (this->Internal->ImageWindowHandle, &rect) ;
  int cx	= rect.right - rect.left;
  int cy	= rect.bottom - rect.top;
  this->Internal->DataHandle = CreateCompatibleBitmap (hdc, cx, cy);  
  GetObject (this->Internal->DataHandle, sizeof (BITMAP), (LPVOID) &this->Internal->Bitmap) ;
  // zero indexed window including borders
  size_t toAllocate=(this->Internal->Bitmap.bmWidth+16) * (this->Internal->Bitmap.bmHeight+4);
  this->Internal->MemoryBitmapBuffer.resize(toAllocate,0);

  std::vector<double> gain;
  this->Internal->ImagingParameters->GetTimeGainCompensation(gain);
  if (gain.size() == 3)
  {
    double tgc[3] = {gain[0], gain[1], gain[2]};
    this->SetTimeGainCompensationPercent(tgc);
  }

  BYTE lut[256];
  double intensity=-1;
  double contrast = -1;
  this->Internal->ImagingParameters->GetIntensity(intensity);
  this->Internal->ImagingParameters->GetContrast(contrast);
  this->Internal->CreateLUT(lut);
  bmCreatebLUT(lut);
  return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::InternalDisconnect()
{
  LOG_DEBUG("Disconnect from Interson");

  this->StopRecording();

  usbProbe (STOP) ;
  Sleep(250); // allow time for the imaging to stop

  // usbStopHardware() should be called here but the issue is that if the method is called, no probe is detected after connecting again. 

  bmCloseDisplay();

  DeleteObject(this->Internal->ProbeHandle);
  DeleteObject(this->Internal->DataHandle);
  DeleteObject(this->Internal->ImageWindowHandle);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::InternalStartRecording()
{
  FreezeDevice(false);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::InternalStopRecording()
{ 
  FreezeDevice(true);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::WaitForFrame()
{
  bool nextFrameReady = (usbWaitFrame()==1);
  DWORD	usbErrorCode = usbError();

  switch (usbErrorCode)
  {
  case USB_SUCCESS:
    break;
  case USB_FAILED:
    LOG_ERROR("USB: FAILURE. Probe was removed?");
    return PLUS_FAIL;
  case USB_TIMEOUT2A:
  case USB_TIMEOUT2B:
  case USB_TIMEOUT6A:
  case USB_TIMEOUT6B:
    if (nextFrameReady) // timeout is fine if we're in synchronized mode, so only log error if next frame is ready
    {
      LOG_WARNING("USB timeout");
    }
    break;
  case USB_NOTSEQ:
    LOG_ERROR("Lost Probe Synchronization. Please check probe cables and restart.");
    break;
  case USB_STOPPED:
    LOG_ERROR("USB: Stopped. Check probe and restart.");
    break;
  default:
    LOG_ERROR("USB: Unknown USB error: "<<usbErrorCode);
    break;
  } 

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::InternalUpdate()
{
  if (!this->Recording)
  {
    // drop the frame, we are not recording data now
    return PLUS_SUCCESS;
  }

  if (usbProbeButtonPressed() && this->Internal->EnableProbeButtonMonitoring)
  {
    this->Internal->ProbeButtonPressCount++;
  }

  WaitForFrame();

  this->Internal->RfDataBuffer = usbCurrentCineFrame();

  bmDrawImage(this->Internal->ImageWindowHandle, this->Internal->RfDataBuffer, this->Internal->Bitmap, &(this->Internal->MemoryBitmapBuffer[0]), &this->Internal->BitmapInfo, this->Internal->Interpolate, this->Internal->BidirectionalScan);

  this->FrameNumber++; 

  vtkPlusDataSource* aSource=NULL;
  if( this->GetFirstActiveOutputVideoSource(aSource) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to retrieve the video source in the ICCapturing device.");
    return PLUS_FAIL;
  }

  int frameSizeInPx[3]={this->Internal->ImageSize[0],this->Internal->ImageSize[1], 1};

  // If the buffer is empty, set the pixel type and frame size to the first received properties 
  if ( aSource->GetNumberOfItems() == 0 )
  {
    LOG_DEBUG("Set up image buffer for Interson");
    aSource->SetPixelType(VTK_UNSIGNED_CHAR);
    aSource->SetImageType(US_IMG_BRIGHTNESS);
    aSource->SetInputFrameSize( frameSizeInPx );

    float depthScale = -1;
    usbProbeDepthScale(this->Internal->ProbeHandle,&depthScale);

    std::string probeName;
    GetProbeNameDevice(probeName);

    LOG_INFO("Frame size: " << frameSizeInPx[0] << "x" << frameSizeInPx[1]
    << ", pixel type: " << vtkImageScalarTypeNameMacro(aSource->GetPixelType())
      << ", probe sample frequency (Hz): " << usbProbeSampleFrequency(this->Internal->ProbeHandle)
      << ", probe name: " << probeName
      << ", display zoom: " << bmDisplayZoom()
      << ", probe depth scale (mm/sample):" << depthScale
      << ", buffer image orientation: " << PlusVideoFrame::GetStringFromUsImageOrientation(aSource->GetInputImageOrientation()) );
  }

  TrackedFrame::FieldMapType customFields;

  if (this->Internal->EnableProbeButtonMonitoring)
  {
    std::ostringstream probeButtonPressCountString;
    probeButtonPressCountString << this->Internal->ProbeButtonPressCount;
    customFields["ProbeButtonToDummyTransform"] =  std::string("1 0 0 ") + probeButtonPressCountString.str() + " 0 1 0 0 0 0 1 0 0 0 0 1";
    customFields["ProbeButtonToDummyTransformStatus"] = "OK";
  }

  if( aSource->AddItem((void*)&(this->Internal->MemoryBitmapBuffer[0]), aSource->GetInputImageOrientation(),
    frameSizeInPx, VTK_UNSIGNED_CHAR, 1, US_IMG_BRIGHTNESS, 0, this->FrameNumber, UNDEFINED_TIMESTAMP, UNDEFINED_TIMESTAMP, &customFields) != PLUS_SUCCESS )
  {
    LOG_ERROR("Error adding item to video source " << aSource->GetSourceId());
    return PLUS_FAIL;
  }
  this->Modified();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::ReadConfiguration(vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);

  if( deviceConfig->GetAttribute("TimeGainCompensationPercent") != NULL )
  {
    double tgc[3];
    deviceConfig->GetVectorAttribute("TimeGainCompensationPercent", 3, tgc);
    std::vector<double> tgcVec;
    tgcVec.assign(tgc, tgc+3);
    this->Internal->ImagingParameters->SetTimeGainCompensation(tgcVec);
  }

  if( deviceConfig->GetAttribute("Intensity") != NULL )
  {
    double intensity;
    deviceConfig->GetScalarAttribute("Intensity", intensity);
    this->Internal->ImagingParameters->SetIntensity(intensity);
  }
  
  if( deviceConfig->GetAttribute("Contrast") != NULL )
  {
    double contrast;
    deviceConfig->GetScalarAttribute("Contrast", contrast);
    this->Internal->ImagingParameters->SetContrast(contrast);
  }

  if( deviceConfig->GetAttribute("DepthMm") != NULL )
  {
    double depthMm;
    deviceConfig->GetScalarAttribute("DepthMm", depthMm);
    this->Internal->ImagingParameters->SetDepthMm(depthMm);
  }

  if( deviceConfig->GetAttribute("SoundVelocity") != NULL )
  {
    double soundVel;
    deviceConfig->GetScalarAttribute("SoundVelocity", soundVel);
    this->Internal->ImagingParameters->SetSoundVelocity(soundVel);
  }

  XML_READ_BOOL_ATTRIBUTE_OPTIONAL(EnableProbeButtonMonitoring, deviceConfig);

  XML_READ_BOOL_ATTRIBUTE_NONMEMBER_OPTIONAL("Interpolate", this->Internal->Interpolate, deviceConfig);
  XML_READ_BOOL_ATTRIBUTE_NONMEMBER_OPTIONAL("BidirectionalScan", this->Internal->BidirectionalScan, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_NONMEMBER_OPTIONAL(int, "ClockDivider", this->Internal->ClockDivider, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_NONMEMBER_OPTIONAL(int, "PulseFrequencyDivider", this->Internal->PulseFrequencyDivider, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_NONMEMBER_OPTIONAL(double, "LutCenter", this->Internal->LutCenter, deviceConfig);
  XML_READ_SCALAR_ATTRIBUTE_NONMEMBER_OPTIONAL(double, "LutWindow", this->Internal->LutWindow, deviceConfig);
  XML_READ_VECTOR_ATTRIBUTE_NONMEMBER_OPTIONAL(int, 2, "ImageSize", this->Internal->ImageSize, deviceConfig);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::WriteConfiguration(vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);

  deviceConfig->SetAttribute("EnableProbeButtonMonitoring", this->Internal->EnableProbeButtonMonitoring?"true":"false");

  std::vector<double> tgcVec;
  this->Internal->ImagingParameters->GetTimeGainCompensation(tgcVec);
  double tgc[3] = {tgcVec[0], tgcVec[1], tgcVec[2]};
  deviceConfig->SetVectorAttribute("TimeGainCompensationPercent", 3, tgc);

  double intensity;
  this->Internal->ImagingParameters->GetIntensity(intensity);
  deviceConfig->SetDoubleAttribute("Intensity", intensity);

  double contrast;
  this->Internal->ImagingParameters->GetContrast(contrast);
  deviceConfig->SetDoubleAttribute("Contrast", contrast);

  double depthMm;
  this->Internal->ImagingParameters->GetDepthMm(depthMm);
  deviceConfig->SetDoubleAttribute("DepthMm", depthMm);

  float soundVel;
  this->Internal->ImagingParameters->GetSoundVelocity(soundVel);
  deviceConfig->SetFloatAttribute("SoundVelocity", soundVel);

  XML_WRITE_BOOL_ATTRIBUTE(EnableProbeButtonMonitoring, deviceConfig);

  XML_WRITE_BOOL_ATTRIBUTE_NONMEMBER("Interpolate", this->Internal->Interpolate, deviceConfig);
  XML_WRITE_BOOL_ATTRIBUTE_NONMEMBER("BidirectionalScan", this->Internal->BidirectionalScan, deviceConfig);
  deviceConfig->SetIntAttribute("ClockDivider", this->Internal->ClockDivider);
  deviceConfig->SetIntAttribute("PulseFrequencyDivider", this->Internal->PulseFrequencyDivider);
  deviceConfig->SetDoubleAttribute("LutCenter", this->Internal->LutCenter);
  deviceConfig->SetDoubleAttribute("LutWindow", this->Internal->LutWindow);
  deviceConfig->SetVectorAttribute("ImageSize", 2, this->Internal->ImageSize);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::NotifyConfigured()
{
  if( this->OutputChannels.size() > 1 )
  {
    LOG_WARNING("vtkIntersonVideoSource is expecting one output channel and there are " << this->OutputChannels.size() << " channels. First output channel will be used.");
  }

  if( this->OutputChannels.empty() )
  {
    LOG_ERROR("No output channels defined for vtkIntersonVideoSource. Cannot proceed." );
    this->CorrectlyConfigured = false;
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::FreezeDevice(bool freeze)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::FreezeDevice failed: device not connected");
    return PLUS_FAIL;
  }
  if (!usbHardwareDetected())
  {
    LOG_ERROR("Freeze failed, no hardware is detected");
    return PLUS_FAIL;
  }
  this->Internal->Frozen=freeze;
  if (this->Internal->Frozen)
  {
    usbProbe(STOP);
  }
  else
  {
    usbClearCineBuffers();
    usbSetProbeFrequency(usbProbeScanFrequency(this->Internal->ProbeHandle));
    usbProbe(RUN);
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
std::string vtkIntersonVideoSource::GetSdkVersion()
{
  std::ostringstream versionString;
  versionString << "Interson Bmode DLL v"<<bmDLLVer()<<", USB Probe DLL v"<<usbDLLVer()<< std::ends;
  return versionString.str();
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::GetSampleFrequencyDevice(double& aFreq)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::GetSampleFrequencyDevice failed: device not connected");
    return PLUS_FAIL;
  }
  aFreq = usbProbeSampleFrequency(this->Internal->ProbeHandle);
  LOG_TRACE("Current frequency is " << aFreq); 
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::GetSoundVelocityDevice(float& aVel)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetWindowDepthDevice failed: device not connected");
    return PLUS_FAIL;
  }
  aVel = usbProbeVelocity(this->Internal->ProbeHandle);
  this->SetSoundVelocity(aVel);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::GetSoundVelocity(double& aVel)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::GetProbeVelocityDevice failed: device not connected");
    return PLUS_FAIL;
  }
  aVel = usbProbeVelocity(this->Internal->ProbeHandle);
  LOG_TRACE("Current velocity is " << aVel); 
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetWindowDepthDevice(int height)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetWindowDepthDevice failed: device not connected");
    return PLUS_FAIL;
  }
  usbSetWindowDepth(this->Internal->ProbeHandle,height);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetDepthMmDevice(double depthMm)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetDepthMmDevice failed: device not connected");
    return PLUS_FAIL;
  }

  std::vector< std::pair<double,double> > allowedModes;
  this->GetProbeAllowedModes(allowedModes);
  int numberOfAllowedModes = allowedModes.size();
  std::vector<int> possibleModes;
  double chosenFrequencyMhz = -1;
  double chosenDepthCm = -1;
  for (int i=0;i<numberOfAllowedModes;i++)
  {
    if(allowedModes[i].second==depthMm/10)
    {
      possibleModes.push_back(i);	
    }
  }
  if(possibleModes.size()==1)
  {
    chosenFrequencyMhz = allowedModes[possibleModes[0]].first ;
    chosenDepthCm = allowedModes[possibleModes[0]].second ;
  }
  else if(possibleModes.size()==0)
  {
    chosenDepthCm = 5 ;
    double clockDivider = usbClockDivider();	
    double sampleFrequency = usbProbeSampleFrequency(this->Internal->ProbeHandle);
    double divider = usbPulseFrequency();
    chosenFrequencyMhz = sampleFrequency / divider ;
    this->Internal->PulseFrequencyDivider = sampleFrequency/chosenFrequencyMhz;
    LOG_INFO("The probe does not allow the required depth." << chosenDepthCm << " cm depth was chosed instead." );
  }

  if ((fabs(chosenDepthCm-5)<TOLERANCE) || fabs(chosenDepthCm-10)<TOLERANCE || fabs(chosenDepthCm-15)<TOLERANCE || fabs(chosenDepthCm-20)<TOLERANCE)
  {
    // select the 30MHz clock
    usbSet30MHzClock(this->Internal->ProbeHandle);
    this->Internal->ClockFrequencyMHz = 30;
    this->Internal->ClockDivider = chosenDepthCm/5;
    // set the clock divider for
    // 1:  ~5cm @ 30MHz and 1540m/s velocity
    // 3: ~15cm @ 30MHz;
    // 4: ~20cm @ 30MHz
  }
  if (fabs(chosenDepthCm-3)<TOLERANCE || fabs(chosenDepthCm-6)<TOLERANCE || fabs(chosenDepthCm-9)<TOLERANCE || fabs(chosenDepthCm-12)<TOLERANCE)
  {
    // select the 48MHz clock
    usbSet48MHzClock(this->Internal->ProbeHandle);
    this->Internal->ClockFrequencyMHz = 48;
    this->Internal->ClockDivider = chosenDepthCm/3;
  }

  this->Internal->PulseFrequencyDivider = this->Internal->ClockFrequencyMHz/chosenFrequencyMhz;
  usbSetPulseFrequency(this->Internal->PulseFrequencyDivider);
  usbSetClockDivider(this->Internal->ProbeHandle, this->Internal->ClockDivider);
  this->SetFrequencyMhz(chosenFrequencyMhz);
  this->Internal->ClockDivider = usbClockDivider(); 
  //pulseFrequency = usbPulseFrequency();
  this->SetDepthMm(depthMm);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetFrequencyMhzDevice(float aFreq)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetProbeFrequencyDevice failed: device not connected");
    return PLUS_FAIL;
  }

  this->SetFrequencyMhz(aFreq);
  usbSetProbeFrequency(aFreq);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetDepthMm(double depthMm)
{
  this->Internal->ImagingParameters->SetDepthMm(depthMm);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetImageSize(int imageSize[2])
{
  this->Internal->ImageSize[0]= imageSize[0];
  this->Internal->ImageSize[1]= imageSize[1];
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
bool vtkIntersonVideoSource::GetEnableProbeButtonMonitoring() const
{
  return this->Internal->EnableProbeButtonMonitoring;
}

//----------------------------------------------------------------------------
void vtkIntersonVideoSource::SetEnableProbeButtonMonitoring(bool _arg)
{
  this->Internal->EnableProbeButtonMonitoring = _arg;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetSoundVelocityDevice(double value)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::GetProbeVelocityDevice failed: device not connected");
    return PLUS_FAIL;
  }

  this->Internal->ImagingParameters->SetSoundVelocity(value);
  usbSetVelocity(this->Internal->ProbeHandle, value);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetSoundVelocity(double value)
{
  this->Internal->ImagingParameters->SetSoundVelocity(value);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetFrequencyMhz(float freq)
{
  this->Internal->ImagingParameters->SetFrequencyMhz(freq);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetSectorPercent(double value)
{
  this->Internal->ImagingParameters->SetSectorPercent(value);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetIntensity(int value)
{
  this->Internal->ImagingParameters->SetIntensity(value);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetContrast(int value)
{
  this->Internal->ImagingParameters->SetContrast(value);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetDynRangeDb(double value)
{
  this->Internal->ImagingParameters->SetDynRangeDb(value);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetTimeGainCompensationPercent(double gainPercent[3])
{
  if (gainPercent[0] < 0 || gainPercent[1] < 0 || gainPercent[2] < 0)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetGainPercentDevice failed: Invalid values sent.")
      return PLUS_FAIL;
  }

  std::vector<double> tgc;
  tgc.assign(gainPercent, gainPercent + 3);
  this->Internal->ImagingParameters->SetTimeGainCompensation(tgc);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetTimeGainCompensationPercentDevice(double gainPercent[3])
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetGainPercentDevice failed: device not connected");
    return PLUS_FAIL;
  }

  if (gainPercent[0] < 0 || gainPercent[1] < 0 || gainPercent[2] < 0)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetGainPercentDevice failed: Invalid values sent.");
    return PLUS_FAIL;
  }
  /* The following commented code is useful when using an RF probe with an analog TGC control.
  It sets the value, in dB, for the gain at the last sample taken.   */
  /*
  initialGain = usbInitialGain();
  midGain = usbMidGain();
  farGain = usbFarGain();
  usbSetInitialGain(this->InitialGain);
  usbSetMidGain(this->MidGain);
  usbSetFarGain(this->FarGain);
  initialGain = usbInitialGain();
  midGain = usbMidGain();
  farGain = usbFarGain();
  */

  /* If the above code is executed the gain values are changed but it has no effect on the image. 
  Probably it is because the probe does not have analog TGC control. 
  The code below sets a linear TGC curve based on three values (initial, middle and end) of the curve.
  */

  const double MAX_TGC(512);

  int nearGain = -255 + gainPercent[0] * MAX_TGC /100 ;
  int midGain = -255 + gainPercent[1] * MAX_TGC /100 ;
  int farGain = -255 + gainPercent[2] * MAX_TGC /100 ;

  this->Internal->CreateLinearTGC(nearGain, midGain, farGain); 

  bmTurnOnTGC();

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetZoomFactor(float zoomFactor)
{
  this->Internal->ImagingParameters->SetZoomFactor(zoomFactor);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetZoomFactorDevice(float zoomFactor)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetDisplayZoomDevice failed: device not connected");
    return PLUS_FAIL;
  }
  this->SetZoomFactor(zoomFactor);
  bmSetDisplayZoom(zoomFactor); 
  LOG_TRACE("New zoom is " << bmDisplayZoom()); 
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::GetProbeAllowedModes(std::vector< std::pair<double,double> > &allowedModes)
{
  double clockDivider = usbClockDivider();	
  double sampleFrequency = usbProbeSampleFrequency(this->Internal->ProbeHandle);
  double divider = usbPulseFrequency();
  unsigned is24MHz, is15MHz, is12MHz, is8MHz, is7_5MHz, is6MHz, is5MHz, is3_75MHz; 
  std::pair<double,double> pair;
  is24MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P24MHZ; 
  if (is24MHz & CLKDIV1) { 
    // now we know the 24 MHz pulse frequency is good and 3 cm mode is good 
    LOG_INFO("24 Mhz pulse frequency and 3 cm depth is an allowed mode for this probe");	  
    pair.first = 24;
    pair.second = 3;
    allowedModes.push_back(pair);
  }
  if (is24MHz & CLKDIV2) { 
    // now we know the 24 MHz pulse frequency is good and 6 cm mode is good 
    LOG_INFO("24 Mhz pulse frequency and 6 cm depth is an allwoed mode for this probe");
  }
  if (is24MHz & CLKDIV3) { 
    // now we know the 24 MHz pulse frequency is good and 9 cm mode is good 
    LOG_INFO("24 Mhz pulse frequency and 9 cm depth is an allwoed mode for this probe");
  }
  if (is24MHz & CLKDIV4) { 
    // now we know the 24 MHz pulse frequency is good and 12 cm mode is good 
    LOG_INFO("24 Mhz pulse frequency and 12 cm depth is an allowed mode for this probe");
  }

  is15MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P15MHZ; 
  if (is15MHz & CLKDIV1) { 
    // now we know the 15 MHz pulse frequency is good and 5 cm mode is good 
    LOG_INFO("15 Mhz pulse frequency and 5 cm depth is an allowed mode for this probe");
  }
  if (is15MHz & CLKDIV2) { 
    // now we know the 15 MHz pulse frequency is good and 10 cm mode is good 
    LOG_INFO("15 Mhz pulse frequency and 10 cm depth is an allowed mode for this probe");
  }
  if (is15MHz & CLKDIV3) { 
    // now we know the 15 MHz pulse frequency is good and 15 cm mode is good 
    LOG_INFO("15 Mhz pulse frequency and 15 cm depth is an allowed mode for this probe");
  }
  if (is15MHz & CLKDIV4) { 
    // now we know the 15 MHz pulse frequency is good and 20 cm mode is good 
    LOG_INFO("15 Mhz pulse frequency and 20 cm depth is an allowed mode for this probe");
  }

  is12MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P12MHZ; 
  if (is12MHz & CLKDIV1) { 
    // now we know the 12 MHz pulse frequency is good and 5 cm mode is good 
    LOG_INFO("12 Mhz pulse frequency and 5 cm depth is an allowed mode for this probe");
  }
  if (is12MHz & CLKDIV2) { 
    // now we know the 12 MHz pulse frequency is good and 10 cm mode is good 
    LOG_INFO("12 Mhz pulse frequency and 10 cm depth is an allowed mode for this probe");
  }
  if (is12MHz & CLKDIV3) { 
    // now we know the 12 MHz pulse frequency is good and 15 cm mode is good 
    LOG_INFO("12 Mhz pulse frequency and 15 cm depth is an allowed mode for this probe");
  }
  if (is12MHz & CLKDIV4) { 
    // now we know the 12 MHz pulse frequency is good and 20 cm mode is good 
    LOG_INFO("12 Mhz pulse frequency and 20 cm depth is an allowed mode for this probe");
  }
  is8MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P8MHZ; 
  if (is8MHz & CLKDIV1) { 
    // now we know the 8 MHz pulse frequency is good and 3 cm mode is good 
    LOG_INFO("8 Mhz pulse frequency and 3 cm depth is an allowed mode for this probe");
  }
  if (is8MHz & CLKDIV2) { 
    // now we know the 8 MHz pulse frequency is good and 6 cm mode is good 
    LOG_INFO("8 Mhz pulse frequency and 6 cm depth is an allowed mode for this probe");
  }
  if (is8MHz & CLKDIV3) { 
    // now we know the 8 MHz pulse frequency is good and 9 cm mode is good 
    LOG_INFO("8 Mhz pulse frequency and 9 cm depth is an allowed mode for this probe");
  }
  if (is8MHz & CLKDIV4) { 
    // now we know the 8 MHz pulse frequency is good and 12 cm mode is good 
    LOG_INFO("8 Mhz pulse frequency and 12 cm depth is an allowed mode for this probe");
  }
  is7_5MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P7_5MHZ; 
  if (is7_5MHz & CLKDIV1) { 
    // now we know the 7.5 MHz pulse frequency is good and 5 cm mode is good 
    LOG_INFO("7.5 Mhz pulse frequency and 5 cm depth is an allowed mode for this probe");
  }
  if (is7_5MHz & CLKDIV2) { 
    // now we know the 7.5 MHz pulse frequency is good and 10 cm mode is good 
    LOG_INFO("7.5 Mhz pulse frequency and 10 cm depth is an allowed mode for this probe");
  }
  if (is7_5MHz & CLKDIV3) { 
    // now we know the 7.5 MHz pulse frequency is good and 15 cm mode is good 
    LOG_INFO("7.5 Mhz pulse frequency and 15 cm depth is an allowed mode for this probe");
  }
  if (is7_5MHz & CLKDIV4) { 
    // now we know the 7.5 MHz pulse frequency is good and 20 cm mode is good 
    LOG_INFO("7.5 Mhz pulse frequency and 20 cm depth is an allowed mode for this probe");
  }
  is6MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P6MHZ; 
  if (is6MHz & CLKDIV1) { 
    // now we know the 6 MHz pulse frequency is good and 3 cm mode is good 
    LOG_INFO("6 Mhz pulse frequency and 3 cm depth is an allowed mode for this probe");
  }
  if (is6MHz & CLKDIV2) { 
    // now we know the 6 MHz pulse frequency is good and 6 cm mode is good 
    LOG_INFO("6 Mhz pulse frequency and 6 cm depth is an allowed mode for this probe");
  }
  if (is6MHz & CLKDIV3) { 
    // now we know the 6 MHz pulse frequency is good and 9 cm mode is good
    LOG_INFO("6 Mhz pulse frequency and 9 cm depth is an allowed mode for this probe");
  }
  if (is6MHz & CLKDIV4) { 
    // now we know the 6 MHz pulse frequency is good and 12 cm mode is good 
    LOG_INFO("6 Mhz pulse frequency and 12 cm depth is an allowed mode for this probe");
  }
  is5MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P5MHZ; 
  if (is5MHz & CLKDIV1) { 
    // now we know the 5 MHz pulse frequency is good and 5 cm mode is good 
    LOG_INFO("5 Mhz pulse frequency and 5 cm depth is an allowed mode for this probe");
  }
  if (is5MHz & CLKDIV2) { 
    // now we know the 5 MHz pulse frequency is good and 10 cm mode is good 
    LOG_INFO("5 Mhz pulse frequency and 10 cm depth is an allowed mode for this probe");
  }
  if (is5MHz & CLKDIV3) { 
    // now we know the 5 MHz pulse frequency is good and 15 cm mode is good 
    LOG_INFO("5 Mhz pulse frequency and 15 cm depth is an allowed mode for this probe");
  }
  if (is5MHz & CLKDIV4) { 
    // now we know the 5 MHz pulse frequency is good and 20 cm mode is good 
    LOG_INFO("5 Mhz pulse frequency and 20 cm depth is an allowed mode for this probe");
  }
  is3_75MHz = usbProbeAllowedModes(this->Internal->ProbeHandle) & P3_75MHZ; 
  if (is3_75MHz & CLKDIV1) { 
    // now we know the 3.75 MHz pulse frequency is good and 5 cm mode is good 
    LOG_INFO("3.75 Mhz pulse frequency and 5 cm depth is an allowed mode for this probe");
    pair.first = 3.75;
    pair.second = 5;
    allowedModes.push_back(pair);
  }
  if (is3_75MHz & CLKDIV2) { 
    // now we know the 3.75 MHz pulse frequency is good and 10 cm mode is good 
    LOG_INFO("3.75 Mhz pulse frequency and 10 cm depth is an allowed mode for this probe");
    pair.first = 3.75;
    pair.second = 10;
    allowedModes.push_back(pair);
  }
  if (is3_75MHz & CLKDIV3) { 
    // now we know the 3.75 MHz pulse frequency is good and 15 cm mode is good
    pair.first = 3.75;
    pair.second = 15;
    allowedModes.push_back(pair);
    LOG_INFO("3.75 Mhz pulse frequency and 15 cm depth is an allowed mode for this probe");
  }
  if (is3_75MHz & CLKDIV4) { 
    // now we know the 3.75 MHz pulse frequency is good and 20 cm mode is good
    pair.first = 3.75;
    pair.second = 20;
    allowedModes.push_back(pair);
    LOG_INFO("3.75 Mhz pulse frequency and 20 cm depth is an allowed mode for this probe");
  }
  //allowedModes = usbProbeAllowedModes(this->Internal->ProbeHandle);
  //LOG_TRACE("Allowed modes are" << allowedModes); 
  return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::GetProbeNameDevice(std::string& probeName)
{
  if (this->Internal->ProbeHandle==NULL)
  {
    LOG_ERROR("vtkIntersonVideoSource::SetGainPercentDevice failed: device not connected");
    return PLUS_FAIL;
  }

  // usbProbeNameString supposed to be able to store the USB probe name
  // but if we use that buffer then it leads to stack corruption,
  // so we use a much larger buffer (the usbProbeNameString buffer size is 20)
  typedef TCHAR usbProbeNameStringSafe[1000];

  usbProbeNameStringSafe probeNameWideStringPtr={0};
  usbProbeName(this->Internal->ProbeHandle, probeNameWideStringPtr);

  // Probe name is stored in a wide-character string, convert it to a multi-byte character string
  char probeNamePrintable[usbProbeNameMaxLength+1]={0};
  wcstombs(probeNamePrintable, (wchar_t*)probeNameWideStringPtr, usbProbeNameMaxLength);

  probeName=probeNamePrintable;

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkIntersonVideoSource::SetNewImagingParametersDevice(const vtkUsImagingParameters& newImagingParameters)
{
  if( this->Internal->ImagingParameters->DeepCopy(newImagingParameters) == PLUS_FAIL )
  {
    LOG_ERROR("Unable to deep copy new imaging parameters.");
    return PLUS_FAIL;
  }

  if( this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_DEPTH) && this->SetDepthMmDevice(this->Internal->ImagingParameters->GetDepthMm()) == PLUS_FAIL )
  {
    return PLUS_FAIL;
  }
  if( this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_FREQUENCY) && this->SetFrequencyMhzDevice(this->Internal->ImagingParameters->GetFrequencyMhz()) == PLUS_FAIL )
  {
    return PLUS_FAIL;
  }
  std::vector<double> tgcVec;
  this->Internal->ImagingParameters->GetTimeGainCompensation(tgcVec);
  double tgc[3] = {tgcVec[0], tgcVec[1], tgcVec[2]};
  if( this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_GAIN) && this->SetTimeGainCompensationPercentDevice(tgc) == PLUS_FAIL )
  {
    return PLUS_FAIL;
  }
  BYTE lut[256];
  if( this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_INTENSITY) && this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_CONTRAST) )
  {
    return PLUS_FAIL;
  }
  this->Internal->CreateLUT(lut);
  bmCreatebLUT(lut);
  if( this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_ZOOM) && this->SetZoomFactorDevice(this->Internal->ImagingParameters->GetZoomFactor()) == PLUS_FAIL )
  {
    return PLUS_FAIL;
  }
  if( this->Internal->ImagingParameters->IsSet(vtkUsImagingParameters::KEY_SOUNDVELOCITY) && this->SetSoundVelocityDevice(this->Internal->ImagingParameters->GetSoundVelocity()) == PLUS_FAIL)
  {
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}