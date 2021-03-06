/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "PlusConfigure.h"
#include "TrackedFrame.h"
#include "vtkDirectory.h"
#include "vtkInteractorStyleTrackballCamera.h"
#include "vtkMath.h"
#include "vtkPlusDevice.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkTrackedFrameList.h"
#include "vtkTransform.h"
#include "vtkVisualizationController.h"
#include "vtkXMLUtilities.h"
#include "vtksys/SystemTools.hxx"
#include <QApplication>
#include <QEvent>
#include <QTimer>
#include <QVTKWidget.h>

//-----------------------------------------------------------------------------

vtkStandardNewMacro(vtkVisualizationController);

//-----------------------------------------------------------------------------

vtkVisualizationController::vtkVisualizationController()
: ImageVisualizer(NULL)
, PerspectiveVisualizer(NULL)
, Canvas(NULL)
, AcquisitionTimer(NULL)
, ResultPolyData(NULL)
, InputPolyData(NULL)
, CurrentMode(DISPLAY_MODE_NONE)
, AcquisitionFrameRate(20)
, TransformRepository(NULL)
, SelectedChannel(NULL)
, DataCollector(NULL)
{
  // Create transform repository
  this->ClearTransformRepository();

  // Input points poly data
  vtkSmartPointer<vtkPolyData> inputPolyData = vtkSmartPointer<vtkPolyData>::New();
  inputPolyData->Initialize();
  vtkSmartPointer<vtkPoints> input = vtkSmartPointer<vtkPoints>::New();
  inputPolyData->SetPoints(input);

  this->SetInputPolyData(inputPolyData);

  // Result points poly data
  vtkSmartPointer<vtkPolyData> resultPolyData = vtkSmartPointer<vtkPolyData>::New();
  resultPolyData->Initialize();
  vtkSmartPointer<vtkPoints> resultPoint = vtkSmartPointer<vtkPoints>::New();
  resultPolyData->SetPoints(resultPoint);
  this->SetResultPolyData(resultPolyData);  

  // Initialize timer
  this->AcquisitionTimer = new QTimer();
  this->AcquisitionTimer->start(1000.0 / this->AcquisitionFrameRate);

  // Create 2D visualizer
  vtkSmartPointer<vtkImageVisualizer> imageVisualizer = vtkSmartPointer<vtkImageVisualizer>::New();
  imageVisualizer->SetResultPolyData(this->ResultPolyData);
  imageVisualizer->EnableROI(false);
  this->SetImageVisualizer(imageVisualizer);

  // Create 3D visualizer
  vtkSmartPointer<vtk3DObjectVisualizer> perspectiveVisualizer = vtkSmartPointer<vtk3DObjectVisualizer>::New();
  perspectiveVisualizer->SetResultPolyData(this->ResultPolyData);
  perspectiveVisualizer->SetInputPolyData(this->InputPolyData);
  this->SetPerspectiveVisualizer(perspectiveVisualizer);

  // Set up blank renderer
  this->BlankRenderer = vtkRenderer::New();
  this->BlankRenderer->SetBackground(0.1, 0.1, 0.1);
  this->BlankRenderer->SetBackground2(0.4, 0.4, 0.4);
  this->BlankRenderer->SetGradientBackground(true);

  connect( this->AcquisitionTimer, SIGNAL( timeout() ), this, SLOT( Update() ) );
}

//-----------------------------------------------------------------------------

vtkVisualizationController::~vtkVisualizationController()
{
  if (this->AcquisitionTimer != NULL)
  {
    disconnect( this->AcquisitionTimer, SIGNAL( timeout() ), this, SLOT( Update() ) );
    this->AcquisitionTimer->stop();
    delete this->AcquisitionTimer;
    this->AcquisitionTimer = NULL;
  } 

  if (this->GetDataCollector() != NULL)
  {
    this->GetDataCollector()->Stop();
    this->GetDataCollector()->Disconnect();
  }
  this->SetDataCollector(NULL);

  this->SetInputPolyData(NULL);
  this->SetResultPolyData(NULL);

  this->SetTransformRepository(NULL);
  this->SetImageVisualizer(NULL);
  this->SetPerspectiveVisualizer(NULL);

  this->SetImageVisualizer(NULL);
  this->SetPerspectiveVisualizer(NULL);

  // Set up blank renderer
  if (this->BlankRenderer != NULL)
  {
    this->BlankRenderer->Delete();
    this->BlankRenderer = NULL;
  }

}

//-----------------------------------------------------------------------------

void vtkVisualizationController::SetCanvas(QVTKWidget* aCanvas)
{
  this->Canvas = aCanvas;
  this->Canvas->setFocusPolicy(Qt::ClickFocus);
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetAcquisitionFrameRate(int aFrameRate)
{
  LOG_TRACE("vtkVisualizationController::SetAcquisitionFrameRate(" << aFrameRate << ")");

  this->AcquisitionFrameRate = aFrameRate;

  if (this->AcquisitionTimer != NULL)
  {
    if (this->AcquisitionTimer->isActive())
    {
      this->AcquisitionTimer->stop();
      this->AcquisitionTimer->start(1000.0 / this->AcquisitionFrameRate);
    }
  }
  else
  {
    LOG_ERROR("Acquisition timer is not initialized!");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::HideAll()
{
  LOG_TRACE("vtkVisualizationController::HideAll");

  // Hide all actors from the renderer
  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->HideAll();
  }
  if( this->ImageVisualizer != NULL )
  {
    this->ImageVisualizer->HideAll();
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ShowInput(bool aOn)
{
  LOG_TRACE("vtkVisualizationController::ShowInput(" << (aOn?"true":"false") << ")");

  if( this->CurrentMode == DISPLAY_MODE_3D && this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->ShowInput(aOn);
    return PLUS_SUCCESS;
  }

  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ShowResult(bool aOn)
{
  LOG_TRACE("vtkVisualizationController::ShowResult(" << (aOn?"true":"false") << ")");

  if( this->CurrentMode == DISPLAY_MODE_3D && this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->ShowResult(aOn);
    return PLUS_SUCCESS;
  }
  else if( this->CurrentMode == DISPLAY_MODE_2D && this->ImageVisualizer != NULL )
  {
    this->ImageVisualizer->ShowResult(aOn);
    return PLUS_SUCCESS;
  }

  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetVisualizationMode( DISPLAY_MODE aMode )
{
  LOG_TRACE("vtkVisualizationController::SetVisualizationMode( DISPLAY_MODE " << (aMode?"true":"false") << ")");

  if (this->GetDataCollector() == NULL)
  {
    LOG_DEBUG("Data collector has not been initialized when visualization mode was changed.");
    return PLUS_FAIL;
  }

  if( this->Canvas == NULL )
  {
    LOG_ERROR("Trying to change visualization mode but no canvas has been assigned for display.");
    return PLUS_FAIL;
  }

  this->DisconnectInput();

  if (aMode == DISPLAY_MODE_2D)
  {
    if (this->SelectedChannel==NULL || this->SelectedChannel->GetVideoDataAvailable() == false)
    {
      LOG_WARNING("Cannot switch to image mode without enabled video in data collector!");
      return PLUS_FAIL;
    }
    if( this->SelectedChannel->GetBrightnessOutput() == NULL )
    {
      LOG_WARNING("No B-mode data available to visualize. Disabling 2D visualization.");
      return PLUS_FAIL;
    }

    if( this->BlankRenderer != NULL && this->GetCanvas()->GetRenderWindow()->HasRenderer(this->BlankRenderer) )
    {
      this->GetCanvas()->GetRenderWindow()->RemoveRenderer(this->BlankRenderer);
    }
    if( this->ImageVisualizer != NULL && this->GetCanvas()->GetRenderWindow()->HasRenderer(this->ImageVisualizer->GetCanvasRenderer()) )
    {
      this->GetCanvas()->GetRenderWindow()->RemoveRenderer(this->ImageVisualizer->GetCanvasRenderer());
    }
    if( this->PerspectiveVisualizer != NULL && this->GetCanvas()->GetRenderWindow()->HasRenderer(this->PerspectiveVisualizer->GetCanvasRenderer()) )
    {
      // If there's already been a renderer added, remove it
      this->GetCanvas()->GetRenderWindow()->RemoveRenderer(this->PerspectiveVisualizer->GetCanvasRenderer());
    }
    // Add the 2D renderer
    if( !GetCanvas()->GetRenderWindow()->HasRenderer(this->ImageVisualizer->GetCanvasRenderer()) )
    {
      this->GetCanvas()->GetRenderWindow()->AddRenderer(this->ImageVisualizer->GetCanvasRenderer());
      this->ImageVisualizer->GetImageActor()->VisibilityOn();
      this->ImageVisualizer->UpdateCameraPose();
    }

    // Disable camera movements
    this->GetCanvas()->GetRenderWindow()->GetInteractor()->RemoveAllObservers();
  }
  else if ( aMode == DISPLAY_MODE_3D )
  {
    if( this->BlankRenderer != NULL && this->GetCanvas()->GetRenderWindow()->HasRenderer(this->BlankRenderer) )
    {
      this->GetCanvas()->GetRenderWindow()->RemoveRenderer(this->BlankRenderer);
    }
    if( this->ImageVisualizer != NULL && this->GetCanvas()->GetRenderWindow()->HasRenderer(this->ImageVisualizer->GetCanvasRenderer()) )
    {
      this->GetCanvas()->GetRenderWindow()->RemoveRenderer(this->ImageVisualizer->GetCanvasRenderer());
    }
    if( this->PerspectiveVisualizer != NULL && this->GetCanvas()->GetRenderWindow()->HasRenderer(this->PerspectiveVisualizer->GetCanvasRenderer()) )
    {
      this->GetCanvas()->GetRenderWindow()->RemoveRenderer(this->PerspectiveVisualizer->GetCanvasRenderer());
    }

    // Add the 3D renderer
    if( !GetCanvas()->GetRenderWindow()->HasRenderer(this->PerspectiveVisualizer->GetCanvasRenderer()) )
    {
      this->GetCanvas()->GetRenderWindow()->AddRenderer(this->PerspectiveVisualizer->GetCanvasRenderer());
    }

    // Enable camera movements
    this->GetCanvas()->GetRenderWindow()->GetInteractor()->SetInteractorStyle(vtkInteractorStyleTrackballCamera::New());
  }
  else
  {
    this->HideRenderer();
  }

  CurrentMode = aMode;

  this->ConnectInput();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ShowOrientationMarkers( bool aShow )
{
  LOG_TRACE("vtkVisualizationController::ShowOrientationMarkers(" << (aShow?"true":"false") << ")");

  if( this->ImageVisualizer != NULL && this->Is2DMode() )
  {
    this->ImageVisualizer->ShowOrientationMarkers(aShow);
  }
  else
  {
    LOG_ERROR("Image visualizer not created or controller is not in 2D mode.");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::StartDataCollection()
{
  LOG_TRACE("vtkVisualizationController::StartDataCollection"); 

  // Delete data collection if already exists
  vtkDataCollector* dataCollector=this->GetDataCollector();
  if (dataCollector != NULL)
  {
    dataCollector->Stop();
    dataCollector->Disconnect();
    this->SetDataCollector(NULL);
  }

  // Create the proper data collector variant
  dataCollector = vtkDataCollector::New();
  this->SetDataCollector(dataCollector);
  dataCollector->Delete();

  // Read configuration
  if (dataCollector->ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }

  if (dataCollector->Connect() != PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }

  if (dataCollector->Start() != PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }

  if (!dataCollector->GetConnected())
  {
    LOG_ERROR("Unable to initialize DataCollector!"); 
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::DumpBuffersToDirectory(const char* aDirectory)
{
  LOG_TRACE("vtkVisualizationController::DumpBuffersToDirectory");

  vtkDataCollector* dataCollector=this->GetDataCollector();
  if (dataCollector==NULL || !dataCollector->GetConnected())
  {
    LOG_INFO("Data collector is not connected, buffers cannot be saved");
    return PLUS_FAIL;
  }

  if(dataCollector->DumpBuffersToDirectory(aDirectory) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to dump data buffers to file.");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

void vtkVisualizationController::resizeEvent( QResizeEvent* aEvent )
{
  LOG_TRACE("vtkVisualizationController::resizeEvent( ... )");
  if( this->ImageVisualizer != NULL)
  {
    this->ImageVisualizer->UpdateCameraPose();
  }
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetScreenRightDownAxesOrientation( US_IMAGE_ORIENTATION aOrientation /*= US_IMG_ORIENT_MF*/ )
{
  LOG_TRACE("vtkVisualizationController::ShowOrientationMarkers(" << aOrientation << ")");

  if( this->ImageVisualizer != NULL)
  {
    this->ImageVisualizer->SetScreenRightDownAxesOrientation(aOrientation);
    return PLUS_SUCCESS;
  }

  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::Update()
{
  if( this->PerspectiveVisualizer != NULL && CurrentMode == DISPLAY_MODE_3D )
  {
    this->PerspectiveVisualizer->Update();
  }

  // Force update of the brightness image in the DataCollector,
  // because it is the image that the image actors show
  if( this->SelectedChannel != NULL && this->GetImageActor() != NULL)
  {
    this->GetImageActor()->SetInputData_vtk5compatible(this->SelectedChannel->GetBrightnessOutput());
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

vtkRenderer* vtkVisualizationController::GetCanvasRenderer()
{
  if( this->CurrentMode == DISPLAY_MODE_3D && this->PerspectiveVisualizer != NULL)
  {
    return PerspectiveVisualizer->GetCanvasRenderer();
  }
  else if ( this->ImageVisualizer != NULL )
  {
    return ImageVisualizer->GetCanvasRenderer();
  }

  return NULL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::EnableVolumeActor( bool aEnable )
{
  if( aEnable )
  {
    if( this->PerspectiveVisualizer != NULL && this->PerspectiveVisualizer->GetVolumeActor() != NULL)
    {
      this->PerspectiveVisualizer->GetVolumeActor()->VisibilityOn();
    }
  }
  else
  {
    if( this->PerspectiveVisualizer != NULL && this->PerspectiveVisualizer->GetVolumeActor() != NULL)
    {
      this->PerspectiveVisualizer->GetVolumeActor()->VisibilityOff();
    }
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::GetTransformTranslationString(const char* aTransformFrom, const char* aTransformTo, std::string &aTransformTranslationString, bool* aValid/* = NULL*/)
{
  PlusTransformName transformName(aTransformFrom, aTransformTo);

  return GetTransformTranslationString(transformName, aTransformTranslationString, aValid);
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::GetTransformTranslationString(PlusTransformName aTransform, std::string &aTransformTranslationString, bool* aValid/* = NULL*/)
{
  vtkSmartPointer<vtkMatrix4x4> transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  if (GetTransformMatrix(aTransform, transformMatrix, aValid) != PLUS_SUCCESS)
  {
    aTransformTranslationString = "N/A";
    return PLUS_FAIL;
  }

  std::stringstream ss;
  ss << std::fixed << std::setprecision(1) << transformMatrix->GetElement(0,3) << " " << transformMatrix->GetElement(1,3) << " " << transformMatrix->GetElement(2,3);

  aTransformTranslationString = ss.str();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::GetTransformMatrix(const char* aTransformFrom, const char* aTransformTo, vtkMatrix4x4* aOutputMatrix, bool* aValid/* = NULL*/)
{
  PlusTransformName transformName(aTransformFrom, aTransformTo);

  return GetTransformMatrix(transformName, aOutputMatrix, aValid);
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::GetTransformMatrix(PlusTransformName aTransform, vtkMatrix4x4* aOutputMatrix, bool* aValid/* = NULL*/)
{
  TrackedFrame trackedFrame;
  if (this->SelectedChannel==NULL || this->SelectedChannel->GetTrackedFrame(&trackedFrame) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to get tracked frame from selected channel!");
    return PLUS_FAIL;
  }
  if (this->TransformRepository->SetTransforms(trackedFrame) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to set transforms from tracked frame!");
    return PLUS_FAIL;
  }

  if (this->TransformRepository->GetTransform(aTransform, aOutputMatrix, aValid) != PLUS_SUCCESS)
  {
    std::string transformName; 
    aTransform.GetTransformName(transformName); 
    LOG_ERROR("Cannot get frame transform '" << transformName << "' from tracked frame!");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetVolumeMapper( vtkPolyDataMapper* aContourMapper )
{
  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->SetVolumeMapper(aContourMapper);
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetVolumeColor( double r, double g, double b )
{
  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->SetVolumeColor(r, g, b);
  }
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetInputColor( double r, double g, double b )
{
  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->SetInputColor(r, g, b);
  }
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::IsExistingTransform(const char* aTransformFrom, const char* aTransformTo, bool aUseLatestTrackedFrame/* = true */)
{
  PlusTransformName transformName(aTransformFrom, aTransformTo);

  if (aUseLatestTrackedFrame)
  {
    if (this->SelectedChannel == NULL || this->SelectedChannel->GetTrackingDataAvailable() == false)
    {
      LOG_WARNING("SelectedChannel object is invalid or the selected channel does not contain tracking data!");
      return PLUS_FAIL;
    }

    TrackedFrame trackedFrame;
    if (this->SelectedChannel->GetTrackedFrame(&trackedFrame) != PLUS_SUCCESS)
    {
      LOG_ERROR("Unable to get tracked frame from data collector!");
      return PLUS_FAIL;
    }
    if (this->TransformRepository->SetTransforms(trackedFrame) != PLUS_SUCCESS)
    {
      LOG_ERROR("Unable to set transforms from tracked frame!");
      return PLUS_FAIL;
    }
  }

  // For debugging purposes
  bool printTransforms = false;
  if (printTransforms)
  {
    this->TransformRepository->PrintSelf(std::cout, vtkIndent());
  }

  return this->TransformRepository->IsExistingTransform(transformName);
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::DisconnectInput()
{
  if( this->GetImageActor() != NULL )
  {
    this->GetImageActor()->SetInputData_vtk5compatible(NULL);
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ConnectInput()
{
  vtkPlusChannel* aChannel(NULL);
  if( this->GetImageActor() != NULL && this->SelectedChannel != NULL )
  {
    this->GetImageActor()->SetInputData_vtk5compatible( this->SelectedChannel->GetBrightnessOutput() );
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

vtkImageActor* vtkVisualizationController::GetImageActor()
{
  if( this->CurrentMode == DISPLAY_MODE_2D && this->ImageVisualizer != NULL )
  {
    return this->ImageVisualizer->GetImageActor();
  }
  else if( this->CurrentMode == DISPLAY_MODE_3D && this->PerspectiveVisualizer != NULL )
  {
    return this->PerspectiveVisualizer->GetImageActor();
  }

  return NULL;
}

//-----------------------------------------------------------------------------

bool vtkVisualizationController::Is2DMode()
{
  return CurrentMode == DISPLAY_MODE_2D;
}

//-----------------------------------------------------------------------------

bool vtkVisualizationController::Is3DMode()
{
  return CurrentMode == DISPLAY_MODE_3D;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::HideRenderer()
{
  LOG_TRACE("vtkVisualizationController::HideRenderer");

  if( this->Canvas == NULL )
  {
    LOG_ERROR("Trying to hide a visualization controller that hasn't been assigned a canvas.");
    return PLUS_FAIL;
  }

  if( this->PerspectiveVisualizer != NULL && Canvas->GetRenderWindow()->HasRenderer(PerspectiveVisualizer->GetCanvasRenderer()) )
  {
    // If there's already been a renderer added, remove it
    this->GetCanvas()->GetRenderWindow()->RemoveRenderer(PerspectiveVisualizer->GetCanvasRenderer());
  }
  if( this->ImageVisualizer != NULL && Canvas->GetRenderWindow()->HasRenderer(ImageVisualizer->GetCanvasRenderer()) )
  {
    // If there's already been a renderer added, remove it
    this->GetCanvas()->GetRenderWindow()->RemoveRenderer(ImageVisualizer->GetCanvasRenderer());
  }

  this->GetCanvas()->GetRenderWindow()->AddRenderer(this->BlankRenderer);  

  this->CurrentMode = DISPLAY_MODE_NONE;

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ShowAllObjects( bool aShow )
{
  if( this->PerspectiveVisualizer != NULL )
  {
    return this->PerspectiveVisualizer->ShowAllObjects(aShow);
  }

  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------
PlusStatus vtkVisualizationController::ReadRoiConfiguration(vtkXMLDataElement* aXMLElement)
{
  if( this->ImageVisualizer == NULL )
  {
    LOG_ERROR("Failed to read ROI configuration, ImageVisualizer is invalid");
    return PLUS_FAIL;
  }
  if( this->ImageVisualizer->ReadRoiConfiguration(aXMLElement) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to configure image visualizer ROI.");
    return PLUS_FAIL;
  }
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ReadConfiguration(vtkXMLDataElement* aXMLElement)
{
  // Fill up transform repository
  if( this->TransformRepository->ReadConfiguration(aXMLElement) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to initialize transform repository!"); 
  }

  // Pass on any configuration steps to children
  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->SetTransformRepository(this->TransformRepository);
    if( this->PerspectiveVisualizer->ReadConfiguration(aXMLElement) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to configure perspective visualizer.");
      return PLUS_FAIL;
    }
  }

  if( this->ImageVisualizer != NULL )
  {
    if( this->ImageVisualizer->ReadConfiguration(aXMLElement) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to configure image visualizer.");
      return PLUS_FAIL;
    }
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::WriteConfiguration(vtkXMLDataElement* aXMLElement)
{
  PlusStatus status = PLUS_SUCCESS;

  if (this->GetDataCollector()->WriteConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to save configuration of data collector"); 
    status = PLUS_FAIL;
  }

  if (this->TransformRepository->WriteConfiguration(aXMLElement) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to save configuration of transform repository"); 
    status = PLUS_FAIL;
  }

  // Here we could give a chance to PerspectiveVisualizer and ImageVisualizer
  // to save configuration parameters, but currently they don't need to save anything.

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::StopAndDisconnectDataCollector()
{
  vtkSmartPointer<vtkDataCollector> dataCollector=this->GetDataCollector();
  if( dataCollector == NULL )
  {
    LOG_WARNING("Trying to disconnect from non-connected data collector.");
    return PLUS_FAIL;
  }

  this->DisconnectInput();
  SetDataCollector(NULL); // the local smart pointer still keeps a reference

  dataCollector->Stop();
  dataCollector->Disconnect();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ClearTransformRepository()
{
  vtkSmartPointer<vtkTransformRepository> transformRepository = vtkSmartPointer<vtkTransformRepository>::New();
  this->SetTransformRepository(transformRepository);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetROIBounds( int xMin, int xMax, int yMin, int yMax )
{
  if( this->ImageVisualizer == NULL )
  {
    LOG_ERROR("Image visualizer not created when attempting to set ROI bounds.");
    return PLUS_FAIL;
  }
  this->ImageVisualizer->SetROIBounds(xMin, xMax, yMin, yMax);
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::EnableROI( bool aEnable )
{
  if( this->ImageVisualizer != NULL )
  {
    this->ImageVisualizer->EnableROI(aEnable);
    return PLUS_SUCCESS;
  }

  LOG_ERROR("Image visualizer not created when attempting to enable the ROI.");
  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::EnableWireLabels( bool aEnable )
{
  LOG_TRACE("vtkVisualizationController::EnableWireLabels");

  if( this->ImageVisualizer != NULL )
  {
    this->ImageVisualizer->EnableWireLabels(aEnable);
    return PLUS_SUCCESS;
  }

  LOG_ERROR("Image visualizer not created when attempting to enable the wire visualization.");
  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::SetWireLabelPositions( vtkPoints* aPointList )
{
  LOG_TRACE("vtkVisualizationController::SetWireLabelPositions");

  if( this->ImageVisualizer != NULL )
  {
    this->ImageVisualizer->SetWireLabelPositions(aPointList);
    return PLUS_SUCCESS;
  }

  LOG_ERROR("Image visualizer not created when attempting to set wire label positions.");
  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::ShowObjectById( const char* aModelId, bool aOn )
{
  LOG_TRACE("vtkVisualizationController::ShowObjectById");

  if( aModelId == NULL )
  {
    return PLUS_FAIL;
  }

  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->ShowObjectById(aModelId, aOn);
    return PLUS_SUCCESS;
  }

  LOG_ERROR("3D visualizer not created when attempting to show an object by ID.");
  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::AddObject(vtkDisplayableObject* aObject)
{
  LOG_TRACE("vtkVisualizationController::AddObject");

  if( aObject == NULL )
  {
    return PLUS_FAIL;
  }

  if( this->PerspectiveVisualizer != NULL )
  {
    this->PerspectiveVisualizer->AddObject(aObject);
    return PLUS_SUCCESS;
  }

  LOG_ERROR("3D visualizer not created when attempting to add an object");
  return PLUS_FAIL;
}

//-----------------------------------------------------------------------------

vtkDisplayableObject* vtkVisualizationController::GetObjectById( const char* aId )
{
  LOG_TRACE("vtkVisualizationController::ShowObjectById");

  if( aId == NULL )
  {
    return NULL;
  }

  if( this->PerspectiveVisualizer != NULL )
  {
    return this->PerspectiveVisualizer->GetObjectById(aId);
  }

  LOG_ERROR("3D visualizer not created when attempting to retrieve an object by ID.");
  return NULL;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVisualizationController::Reset()
{
  PlusStatus perspective, image;
  if( this->PerspectiveVisualizer != NULL )
  {
    perspective = this->PerspectiveVisualizer->ClearDisplayableObjects();
  }
  if( this->ImageVisualizer != NULL )
  {
    image = this->ImageVisualizer->Reset();
  }

  return (perspective == PLUS_SUCCESS && image == PLUS_SUCCESS) ? PLUS_SUCCESS : PLUS_FAIL;
}

//-----------------------------------------------------------------------------

void vtkVisualizationController::SetInputData( vtkImageData * input )
{
  if( this->ImageVisualizer != NULL )
  {
    this->GetImageVisualizer()->SetInputData(input);
  }
}

//-----------------------------------------------------------------------------

void vtkVisualizationController::SetSelectedChannel( vtkPlusChannel* aChannel )
{
  this->SelectedChannel = aChannel;

  if( this->ImageVisualizer != NULL )
  {
    this->GetImageVisualizer()->SetChannel(aChannel);
  }

  if( this->PerspectiveVisualizer != NULL )
  {
    this->GetPerspectiveVisualizer()->SetChannel(aChannel);
  }
}

//-----------------------------------------------------------------------------
void vtkVisualizationController::SetSliceNumber(int number)
{
  if( this->ImageVisualizer != NULL )
  {
    this->GetImageVisualizer()->SetSliceNumber(number);
  }
  if( this->PerspectiveVisualizer != NULL )
  {
    this->GetPerspectiveVisualizer()->SetSliceNumber(number);
  }
}
