/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "vtkConeSource.h"
#include "vtkImageVisualizer.h"
#include "vtkLineSource.h"
#include "vtkObjectFactory.h"
#include "vtkPlusDevice.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkSphereSource.h"
#include "vtkTextProperty.h"
#include "vtkImageSliceMapper.h"

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkImageVisualizer);
//-----------------------------------------------------------------------------

double vtkImageVisualizer::ROI_COLOR[3] = {1.0, 0.0, 0.5};
static double RESULT_SPHERE_COLOR[3] = {0.0, 0.8, 0.0};
static const double MAX_WIDGET_THICKNESS = 10.0;  // maximum thickness of any object in the scene (camera is positioned at -MAX_WIDGET_THICKNESS - 1
static double HORIZONTAL_TEXT_ORIENTATION_MARKER_OFFSET[3] = {30.0, 17.0, -1.0};
static double VERTICAL_TEXT_ORIENTATION_MARKER_OFFSET[3] = {4.0, 40.0, -1.0};
static double ORIENTATION_MARKER_COLOR[3] = {0.0, 1.0, 0.0};
static double ORIENTATION_MARKER_SIZE = 51.0;
static double ORIENTATION_MARKER_ASSEMBLY_POSITION[3] = {12.0, 12.0, -1.0};
static const double ORIENTATION_MARKER_CONE_RADIUS = 5.0;
static const double ORIENTATION_MARKER_CONE_HEIGHT = 15.0;

//-----------------------------------------------------------------------------

vtkImageVisualizer::vtkImageVisualizer()
: CanvasRenderer(NULL)
, ImageActor(NULL)
, ResultActor(NULL)
, ResultGlyph(NULL)
, ImageCamera(NULL)
, OrientationMarkerAssembly(NULL)
, HorizontalOrientationTextActor(NULL)
, VerticalOrientationTextActor(NULL)
, ScreenAlignedCurrentXRotation(0.0)
, ScreenAlignedCurrentYRotation(0.0)
, CurrentMarkerOrientation(US_IMG_ORIENT_MF)
, ScreenAlignedProps(NULL)
, ROIActorAssembly(NULL)
, LeftLineSource(NULL)
, TopLineSource(NULL)
, RightLineSource(NULL)
, BottomLineSource(NULL)
, SelectedChannel(NULL)
{
  this->RegionOfInterest[0] = this->RegionOfInterest[1] = this->RegionOfInterest[2] = this->RegionOfInterest[3] = -1;

  vtkSmartPointer<vtkProp3DCollection> screenAlignedProps = vtkSmartPointer<vtkProp3DCollection>::New();
  this->SetScreenAlignedProps(screenAlignedProps);

  // Set up canvas renderer
  vtkSmartPointer<vtkRenderer> canvasRenderer = vtkSmartPointer<vtkRenderer>::New(); 
  canvasRenderer->SetBackground(0.1, 0.1, 0.1);
  canvasRenderer->SetBackground2(0.4, 0.4, 0.4);
  canvasRenderer->SetGradientBackground(false);
  this->SetCanvasRenderer(canvasRenderer);

  // Create Glyph and Actor
  vtkSmartPointer<vtkActor> resultActor = vtkSmartPointer<vtkActor>::New();
  this->SetResultActor(resultActor);
  vtkSmartPointer<vtkGlyph3D> resultGlyph = vtkSmartPointer<vtkGlyph3D>::New();
  this->SetResultGlyph(resultGlyph);
  vtkSmartPointer<vtkPolyDataMapper> resultMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  vtkSmartPointer<vtkSphereSource> resultSphereSource = vtkSmartPointer<vtkSphereSource>::New();
  resultSphereSource->SetRadius(3.0); // mm

  this->ResultGlyph->SetSourceConnection(resultSphereSource->GetOutputPort());
  resultMapper->SetInputConnection(this->ResultGlyph->GetOutputPort());
  this->ResultActor->SetMapper(resultMapper);
  this->ResultActor->GetProperty()->SetColor(RESULT_SPHERE_COLOR);

  // Create image actor
  vtkSmartPointer<vtkImageActor> imageActor = vtkSmartPointer<vtkImageActor>::New();
  this->SetImageActor(imageActor);

  this->ImageMapper = vtkImageSliceMapper::SafeDownCast(this->ImageActor->GetMapper());

  // Add actors to the renderer
  this->CanvasRenderer->AddActor(this->ResultActor);
  this->CanvasRenderer->AddActor(this->ImageActor);

  this->InitializeOrientationMarkers();

  this->InitializeROIVisualization();
}

//-----------------------------------------------------------------------------

vtkImageVisualizer::~vtkImageVisualizer()
{
  ClearScreenAlignedActorList();

  this->SetScreenAlignedProps(NULL);

  this->SetResultActor(NULL);
  this->SetCanvasRenderer(NULL);
  this->SetImageActor(NULL);
  this->SetImageCamera(NULL);
  this->SetHorizontalOrientationTextActor(NULL);
  this->SetVerticalOrientationTextActor(NULL);
  this->SetROIActorAssembly(NULL);

  if (this->LeftLineSource != NULL) {
    this->LeftLineSource->Delete();
    this->LeftLineSource = NULL;
  }

  if (this->TopLineSource != NULL) {
    this->TopLineSource->Delete();
    this->TopLineSource = NULL;
  }

  if (this->RightLineSource != NULL) {
    this->RightLineSource->Delete();
    this->RightLineSource = NULL;
  }

  if (this->BottomLineSource != NULL) {
    this->BottomLineSource->Delete();
    this->BottomLineSource = NULL;
  }
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::InitializeOrientationMarkers()
{
  vtkSmartPointer<vtkAssembly> assembly = vtkSmartPointer<vtkAssembly>::New();
  this->SetOrientationMarkerAssembly(assembly);

  vtkSmartPointer<vtkTextActor3D> horizontalOrientationTextActor = vtkSmartPointer<vtkTextActor3D>::New();
  horizontalOrientationTextActor->GetTextProperty()->SetColor(ORIENTATION_MARKER_COLOR);
  horizontalOrientationTextActor->GetTextProperty()->SetFontFamilyToArial();
  horizontalOrientationTextActor->GetTextProperty()->SetFontSize(16);
  horizontalOrientationTextActor->GetTextProperty()->SetJustificationToLeft();
  horizontalOrientationTextActor->GetTextProperty()->SetVerticalJustificationToTop();
  horizontalOrientationTextActor->GetTextProperty()->BoldOn(); 
  horizontalOrientationTextActor->SetInput("M");
  horizontalOrientationTextActor->RotateWXYZ(180.0, 1.0, 0.0, 0.0);
  horizontalOrientationTextActor->SetPosition(HORIZONTAL_TEXT_ORIENTATION_MARKER_OFFSET);
  this->SetHorizontalOrientationTextActor(horizontalOrientationTextActor);
  this->OrientationMarkerAssembly->AddPart(horizontalOrientationTextActor);

  vtkSmartPointer<vtkTextActor3D> verticalOrientationTextActor = vtkSmartPointer<vtkTextActor3D>::New();
  verticalOrientationTextActor->GetTextProperty()->SetColor(ORIENTATION_MARKER_COLOR);
  verticalOrientationTextActor->GetTextProperty()->SetFontFamilyToArial();
  verticalOrientationTextActor->GetTextProperty()->SetFontSize(16);
  verticalOrientationTextActor->GetTextProperty()->SetJustificationToLeft();
  verticalOrientationTextActor->GetTextProperty()->SetVerticalJustificationToTop();
  verticalOrientationTextActor->GetTextProperty()->BoldOn(); 
  verticalOrientationTextActor->SetInput("F");
  verticalOrientationTextActor->RotateWXYZ(180.0, 1.0, 0.0, 0.0); 
  verticalOrientationTextActor->SetPosition(VERTICAL_TEXT_ORIENTATION_MARKER_OFFSET);
  this->SetVerticalOrientationTextActor(verticalOrientationTextActor);
  this->OrientationMarkerAssembly->AddPart(verticalOrientationTextActor);

  vtkSmartPointer<vtkActor> horizontalLineActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> horizontalLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  vtkSmartPointer<vtkLineSource> horizontalLineSource = vtkSmartPointer<vtkLineSource>::New();
  horizontalLineSource->SetPoint1(0.0, 0.0, -0.5);
  double horizontal[3] = {1.0, 0.0, -0.5};
  horizontal[0] *= (ORIENTATION_MARKER_SIZE * 0.9);
  horizontalLineSource->SetPoint2(horizontal);
  horizontalLineMapper->SetInputConnection(horizontalLineSource->GetOutputPort());
  horizontalLineActor->GetProperty()->SetColor(ORIENTATION_MARKER_COLOR);
  horizontalLineActor->SetMapper(horizontalLineMapper);
  this->OrientationMarkerAssembly->AddPart(horizontalLineActor);

  vtkSmartPointer<vtkActor> verticalLineActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> verticalLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  vtkSmartPointer<vtkLineSource> verticalLineSource = vtkSmartPointer<vtkLineSource>::New();
  verticalLineSource->SetPoint1(0.0, 0.0, -0.5);
  double vertical[3] = {0.0, 1.0, -0.5};
  vertical[1] *= (ORIENTATION_MARKER_SIZE * 0.9);
  verticalLineSource->SetPoint2(vertical);
  verticalLineMapper->SetInputConnection(verticalLineSource->GetOutputPort());
  verticalLineActor->GetProperty()->SetColor(ORIENTATION_MARKER_COLOR);
  verticalLineActor->SetMapper(verticalLineMapper);
  this->OrientationMarkerAssembly->AddPart(verticalLineActor);

  vtkSmartPointer<vtkActor> horizontalConeActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> horizontalConeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  vtkSmartPointer<vtkConeSource> horizontalConeSource = vtkSmartPointer<vtkConeSource>::New();
  horizontalConeSource->SetHeight(ORIENTATION_MARKER_CONE_HEIGHT);
  horizontalConeSource->SetRadius(ORIENTATION_MARKER_CONE_RADIUS);
  horizontalConeMapper->SetInputConnection(horizontalConeSource->GetOutputPort());
  horizontalConeActor->GetProperty()->SetColor(ORIENTATION_MARKER_COLOR);
  horizontalConeActor->RotateWXYZ(90.0, 1.0, 0.0, 0.0); 
  horizontal[0] = ORIENTATION_MARKER_SIZE;
  horizontalConeActor->SetPosition(horizontal);
  horizontalConeActor->SetMapper(horizontalConeMapper);
  this->OrientationMarkerAssembly->AddPart(horizontalConeActor);

  vtkSmartPointer<vtkActor> verticalConeActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> verticalConeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  vtkSmartPointer<vtkConeSource> verticalConeSource = vtkSmartPointer<vtkConeSource>::New();
  verticalConeSource->SetHeight(ORIENTATION_MARKER_CONE_HEIGHT);
  verticalConeSource->SetRadius(ORIENTATION_MARKER_CONE_RADIUS);
  verticalConeMapper->SetInputConnection(verticalConeSource->GetOutputPort());
  verticalConeActor->GetProperty()->SetColor(ORIENTATION_MARKER_COLOR);
  verticalConeActor->RotateWXYZ(90.0, 1.0, 0.0, 0.0); 
  verticalConeActor->RotateWXYZ(90.0, 0.0, 0.0, 1.0); 
  vertical[1] = ORIENTATION_MARKER_SIZE;
  verticalConeActor->SetPosition(vertical);
  verticalConeActor->SetMapper(verticalConeMapper);
  this->OrientationMarkerAssembly->AddPart(verticalConeActor);

  this->OrientationMarkerAssembly->SetPosition(ORIENTATION_MARKER_ASSEMBLY_POSITION);

  this->AddScreenAlignedProp(assembly);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::UpdateOrientationMarkerLabelling()
{
  // Force redraw of vtkTextActor3D (it's necessary probably
  // due to a bug in VTK)
  this->GetHorizontalOrientationTextActor()->SetInput("AA");
  this->GetHorizontalOrientationTextActor()->GetBounds();
  this->GetVerticalOrientationTextActor()->SetInput("AA");
  this->GetVerticalOrientationTextActor()->GetBounds();

  // Change the letters on the display to indicate the new orientation
  switch(this->CurrentMarkerOrientation)
  {
  case US_IMG_ORIENT_MF:
    this->GetHorizontalOrientationTextActor()->SetInput("M");
    this->GetVerticalOrientationTextActor()->SetInput("F");
    break;
  case US_IMG_ORIENT_MN: 
    this->GetHorizontalOrientationTextActor()->SetInput("M");
    this->GetVerticalOrientationTextActor()->SetInput("N");
    break;
  case US_IMG_ORIENT_UN:    
    this->GetHorizontalOrientationTextActor()->SetInput("U");
    this->GetVerticalOrientationTextActor()->SetInput("N");
    break;
  case US_IMG_ORIENT_UF:    
    this->GetHorizontalOrientationTextActor()->SetInput("U");
    this->GetVerticalOrientationTextActor()->SetInput("F");
    break;
  case US_IMG_ORIENT_FM:
    this->GetHorizontalOrientationTextActor()->SetInput("F");
    this->GetVerticalOrientationTextActor()->SetInput("M");
    break;
  case US_IMG_ORIENT_FU:
    this->GetHorizontalOrientationTextActor()->SetInput("F");
    this->GetVerticalOrientationTextActor()->SetInput("U");
    break;
  case US_IMG_ORIENT_NM:
    this->GetHorizontalOrientationTextActor()->SetInput("N");
    this->GetVerticalOrientationTextActor()->SetInput("M");
    break;
  case US_IMG_ORIENT_NU:
    this->GetHorizontalOrientationTextActor()->SetInput("N");
    this->GetVerticalOrientationTextActor()->SetInput("U");
    break;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::UpdateCameraPose()
{
  LOG_TRACE("vtkImageVisualizer::UpdateCameraPose");

  // Only set new camera if image actor is visible and data collector is connected
  if ((this->ImageActor == NULL) || (this->ImageActor->GetVisibility() == 0) || (this->SelectedChannel==NULL) )
  {
    return PLUS_SUCCESS;
  }

  // Calculate image center
  int dimensions[3]={0,0,0};
  this->SelectedChannel->GetBrightnessFrameSize(dimensions);
  double imageCenterX = dimensions[0] / 2.0;
  double imageCenterY = dimensions[1] / 2.0;
  double imageCenterZ = dimensions[2] / 2.0;

  // Set up camera
  vtkSmartPointer<vtkCamera> imageCamera = vtkSmartPointer<vtkCamera>::New(); 
  imageCamera->SetFocalPoint(imageCenterX, imageCenterY, 0);
  imageCamera->SetViewUp(0, -1, 0);
  imageCamera->ParallelProjectionOn();

  // Calculate distance of camera from the plane
  if( this->CanvasRenderer->GetRenderWindow() == NULL)
  {
    LOG_ERROR("Render window unavailable when trying to render 2D image.");
    return PLUS_FAIL;
  }
  int *renderWindowSize = this->CanvasRenderer->GetRenderWindow()->GetSize();
  if ((double)renderWindowSize[0] / (double)renderWindowSize[1] > imageCenterX / imageCenterY)
  {
    // If canvas aspect ratio is more elongated in the X position then compute the distance according to the Y axis
    imageCamera->SetParallelScale(imageCenterY);
  }
  else
  {
    imageCamera->SetParallelScale(imageCenterX * (double)renderWindowSize[1] / (double)renderWindowSize[0]);
  }

  imageCamera->SetPosition(imageCenterX, imageCenterY, -MAX_WIDGET_THICKNESS - 1);

  // Set camera
  this->SetImageCamera(imageCamera);

  this->CanvasRenderer->SetActiveCamera(this->ImageCamera);

  // Depending on the desired orientation either roll the camera about its view vector 
  // and/or position the camera on the +z or -z side of the image plane.
  // This depends on having back face culling disabled.
  double cameraPos[3] = {imageCenterX, imageCenterY, -MAX_WIDGET_THICKNESS - 1};
  switch(this->CurrentMarkerOrientation)
  {
  case US_IMG_ORIENT_UN:
    {
      //Unmarked, near
      this->ImageCamera->SetRoll(0);
      if( cameraPos[2] > 0.0 )
      {
        this->ImageCamera->SetPosition(cameraPos[0], cameraPos[1], -cameraPos[2]);
      }
    }
    break;
  case US_IMG_ORIENT_UF:
    {
      // Unmarked, far
      this->ImageCamera->SetRoll(-180);
      if( cameraPos[2] < 0.0 )
      {
        this->ImageCamera->SetPosition(cameraPos[0], cameraPos[1], -cameraPos[2]);
      }
    }
    break;
  case US_IMG_ORIENT_MF:
    {
      // Marked, far
      this->ImageCamera->SetRoll(-180);
      if( cameraPos[2] > 0.0 )
      {
        this->ImageCamera->SetPosition(cameraPos[0], cameraPos[1], -cameraPos[2]);
      }
    }
    break;
  case US_IMG_ORIENT_MN:
    {
      // Marked, near
      this->ImageCamera->SetRoll(0);
      if( cameraPos[2] < 0.0 )
      {
        this->ImageCamera->SetPosition(cameraPos[0], cameraPos[1], -cameraPos[2]);
      }
    }
    break;
  }

  if( this->UpdateScreenAlignedActors() != PLUS_SUCCESS )
  {
    LOG_ERROR("Error during alignment of screen-aligned actors.");
    return PLUS_FAIL;
  }

  this->SetROIBounds(this->RegionOfInterest[0], this->RegionOfInterest[1], this->RegionOfInterest[2], this->RegionOfInterest[3]);

  return UpdateOrientationMarkerLabelling();
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::SetScreenRightDownAxesOrientation( US_IMAGE_ORIENTATION aOrientation /*= US_IMG_ORIENT_MF*/ )
{
  LOG_TRACE("vtkImageVisualizer::SetScreenRightDownAxesOrientation(" << aOrientation << ")");

  this->CurrentMarkerOrientation = aOrientation;

  vtkXMLDataElement* renderingParameters = PlusXmlUtils::GetNestedElementWithName(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData(), "Rendering");
  if (renderingParameters == NULL) {
    LOG_ERROR("No Rendering element is found in the XML tree!");
    return PLUS_FAIL;
  }

  const char * orientationValue = PlusVideoFrame::GetStringFromUsImageOrientation(aOrientation);
  renderingParameters->SetAttribute("DisplayedImageOrientation", orientationValue);

  return UpdateCameraPose();
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::ShowOrientationMarkers( bool aShow )
{
  if( !aShow )
  {
    this->OrientationMarkerAssembly->VisibilityOff();
  }
  else
  {
    this->OrientationMarkerAssembly->VisibilityOn();
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::HideAll()
{
  this->ResultActor->VisibilityOff();
  this->ImageActor->VisibilityOff();

  this->CanvasRenderer->Modified();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::ShowResult(bool aOn)
{
  this->ResultActor->SetVisibility(aOn);
  this->CanvasRenderer->Modified();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::SetResultColor(double r, double g, double b)
{
  this->ResultActor->GetProperty()->SetColor(r, g, b);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::SetResultOpacity(double aOpacity)
{
  this->ResultActor->GetProperty()->SetOpacity(aOpacity);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

void vtkImageVisualizer::SetInputData(vtkImageData* aImage )
{
  LOG_TRACE("vtkImageVisualizer::SetInputData");

  this->GetImageActor()->SetInputData_vtk5compatible(aImage);
}

//-----------------------------------------------------------------------------
void vtkImageVisualizer::SetResultPolyData(vtkPolyData* aResultPolyData )
{
  LOG_TRACE("vtkImageVisualizer::SetResultPolyData");
  this->ResultGlyph->SetInputData_vtk5compatible(aResultPolyData);
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::AddScreenAlignedProp(vtkProp3D* aProp )
{
  LOG_TRACE("vtkImageVisualizer::AddScreenAlignedProp");

  // Store the prop for later manipulation
  this->ScreenAlignedProps->AddItem(aProp);

  // Store the original position of the prop
  std::vector<double> pos;
  pos.push_back(aProp->GetPosition()[0]);
  pos.push_back(aProp->GetPosition()[1]);
  pos.push_back(aProp->GetPosition()[2]);
  this->ScreenAlignedPropOriginalPosition.push_back(pos);

  // Add it to the canvas
  this->GetCanvasRenderer()->AddActor(aProp);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::RemoveScreenAlignedProp( vtkProp3D* aProp )
{
  // Find index of aProp
  int i = 0;
  {
    vtkProp3D* prop = NULL;
    this->ScreenAlignedProps->InitTraversal();

    do
    {
      prop = this->ScreenAlignedProps->GetNextProp3D();
      if( prop == NULL || prop == aProp )
      {
        break;
      }
      ++i;
    }
    while(prop != NULL);
  }

  if( i > this->ScreenAlignedProps->GetNumberOfItems() )
  {
    LOG_ERROR("Prop not found in screen aligned prop list.");
    return PLUS_FAIL;
  }

  this->ScreenAlignedProps->RemoveItem(aProp);
  this->ScreenAlignedPropOriginalPosition.erase(this->ScreenAlignedPropOriginalPosition.begin() + i);

  this->GetCanvasRenderer()->RemoveActor(aProp);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::ClearScreenAlignedActorList()
{
  LOG_TRACE("vtkImageVisualizer::ClearScreenAlignedActorList");

  vtkProp3D *prop=NULL;
  vtkCollectionSimpleIterator pit;
  for ( this->ScreenAlignedProps->InitTraversal(pit);  prop = this->ScreenAlignedProps->GetNextProp3D(pit); )
  {
    this->GetCanvasRenderer()->RemoveActor(prop);
  }

  this->ScreenAlignedPropOriginalPosition.clear();
  this->ScreenAlignedProps->RemoveAllItems();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::UpdateScreenAlignedActors()
{
  LOG_TRACE("vtkImageVisualizer::UpdateScreenAlignedActors");

  // Only set new camera if image actor is visible and data collector is connected
  if (this->SelectedChannel==NULL)
  {
    return PLUS_SUCCESS;
  }

  int dimensions[3]={0,0,0};
  this->SelectedChannel->GetBrightnessFrameSize(dimensions);    

  vtkCollectionSimpleIterator pit;
  vtkProp3D *prop=NULL;
  int i = 0;
  for ( this->ScreenAlignedProps->InitTraversal(pit); prop = this->ScreenAlignedProps->GetNextProp3D(pit); ++i)
  {
    // Undo any existing rotations to the prop, so that all subsequent rotations are from a base orientation
    // Base orientation is MF_SCREEN_RIGHT_DOWN
    prop->RotateX(-this->ScreenAlignedCurrentXRotation);
    prop->RotateY(-this->ScreenAlignedCurrentYRotation);

    // Future work: instead of saving the original position of a prop (which will prevent any clients from changing the position of the prop after adding it)
    // determine a method to back calculate the original position
    // Possible solution is to pass in the previous orientation, then reverse the following calculations
    double originalPosition[3]={0};
    originalPosition[0] = this->ScreenAlignedPropOriginalPosition.at(i).at(0);
    originalPosition[1] = this->ScreenAlignedPropOriginalPosition.at(i).at(1);
    originalPosition[2] = this->ScreenAlignedPropOriginalPosition.at(i).at(2);      

    // Apply any necessary rotations and repositioning
    double newPosition[3]={0};
    switch(this->CurrentMarkerOrientation)
    {
    case US_IMG_ORIENT_MF:
      newPosition[0] = originalPosition[0];
      newPosition[1] = originalPosition[1];
      newPosition[2] = originalPosition[2];
      break;
    case US_IMG_ORIENT_MN:
      prop->RotateX(180);
      newPosition[0] = originalPosition[0];
      newPosition[1] = dimensions[1] - originalPosition[1];
      newPosition[2] = -originalPosition[2];
      break;
    case US_IMG_ORIENT_UN:
      prop->RotateX(180);
      prop->RotateY(180);
      newPosition[0] = dimensions[0] - originalPosition[0];
      newPosition[1] = dimensions[1] - originalPosition[1];
      newPosition[2] = originalPosition[2];
      break;
    case US_IMG_ORIENT_UF:
      prop->RotateY(180);
      newPosition[0] = dimensions[0] - originalPosition[0];
      newPosition[1] = originalPosition[1];
      newPosition[2] = -originalPosition[2];
      break;
    }

    prop->SetPosition(newPosition);
  }

  // Now update the member variables so that future rotations correctly undo the current rotations
  switch(this->CurrentMarkerOrientation)
  {
  case US_IMG_ORIENT_MF:
    this->ScreenAlignedCurrentXRotation = 0;
    this->ScreenAlignedCurrentYRotation = 0;
    break;
  case US_IMG_ORIENT_MN:
    this->ScreenAlignedCurrentYRotation = 0;
    this->ScreenAlignedCurrentXRotation = 180;
    break;
  case US_IMG_ORIENT_UN:
    this->ScreenAlignedCurrentXRotation = 180.0;
    this->ScreenAlignedCurrentYRotation = 180.0;
    break;
  case US_IMG_ORIENT_UF:
    this->ScreenAlignedCurrentYRotation = 180.0;
    this->ScreenAlignedCurrentXRotation = 0;
    break;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkImageVisualizer::ReadRoiConfiguration(vtkXMLDataElement* aXMLElement)
{
  //Find segmentation parameters element
  vtkXMLDataElement* segmentationParameters = aXMLElement->FindNestedElementWithName("Segmentation");
  if (segmentationParameters == NULL)
  {
    LOG_WARNING("No Segmentation element is found in the XML tree!");
    this->EnableROI(false);
    return PLUS_FAIL;
  }
  // clipping parameters
  int clipRectangleOrigin[2]={-1, -1};
  if (!segmentationParameters->GetVectorAttribute("ClipRectangleOrigin", 2, clipRectangleOrigin))
  {
    LOG_WARNING("Cannot find ClipRectangleOrigin attribute in the segmentation parameters section of the configuration, region of interest will not be displayed");
  }
  int clipRectangleSize[2]={-1, -1};
  if (!segmentationParameters->GetVectorAttribute("ClipRectangleSize", 2, clipRectangleSize))
  {
    LOG_WARNING("Cannot find ClipRectangleSize attribute in the segmentation parameters section of the configuration, region of interest will not be displayed");
  }
  if (clipRectangleOrigin[0]>=0 && clipRectangleOrigin[1]>=0 && clipRectangleSize[0]>0 && clipRectangleSize[1]>0)
  {
    this->SetROIBounds(clipRectangleOrigin[0], clipRectangleOrigin[0]+clipRectangleSize[0], clipRectangleOrigin[1], clipRectangleOrigin[1]+clipRectangleSize[1]);
  } 
  else
  {
    LOG_DEBUG("Region of interest will not be displayed until valid values are specified");
    this->EnableROI(false);
  }

  return PLUS_SUCCESS;
}


//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::ReadConfiguration( vtkXMLDataElement* aConfig )
{
  LOG_TRACE("vtkImageVisualizer::ReadConfiguration");

  // Rendering section
  vtkXMLDataElement* xmlElement = aConfig->FindNestedElementWithName("Rendering"); 

  if (xmlElement == NULL)
  {
    LOG_ERROR("Unable to find Rendering element in XML tree!"); 
    return PLUS_FAIL;     
  }

  // Displayed image orientation
  const char* orientation = xmlElement->GetAttribute("DisplayedImageOrientation");
  US_IMAGE_ORIENTATION orientationValue = PlusVideoFrame::GetUsImageOrientationFromString(orientation);
  if( orientationValue == US_IMG_ORIENT_XX )
  {
    LOG_WARNING("Unable to read image orientation from configuration file (Rendering tag, DisplayedImageOrientation attribute). Defauting to MF.");
    orientationValue = US_IMG_ORIENT_MF;
  }
  this->CurrentMarkerOrientation = orientationValue;

  ReadRoiConfiguration(aConfig);

  if( InitializeWireLabelVisualization(aConfig) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to initialize wire label visualization.");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::SetROIBounds( int xMin, int xMax, int yMin, int yMax )
{
  LOG_TRACE("vtkImageVisualizer::SetROIBounds");

  if (xMin > 0) {
    this->RegionOfInterest[0] = xMin;
  }
  if (xMax > 0) {
    this->RegionOfInterest[1] = xMax;
  }
  if (yMin > 0) {
    this->RegionOfInterest[2] = yMin;
  }
  if (yMax > 0) {
    this->RegionOfInterest[3] = yMax;
  }

  double zPos = -1.0;
  if( this->CurrentMarkerOrientation == US_IMG_ORIENT_MN || this->CurrentMarkerOrientation == US_IMG_ORIENT_UF )
  {
    zPos = 1.0;
  }

  // Set line positions
  LeftLineSource->SetPoint1(this->RegionOfInterest[0], this->RegionOfInterest[2], zPos);
  LeftLineSource->SetPoint2(this->RegionOfInterest[0], this->RegionOfInterest[3],  zPos);
  TopLineSource->SetPoint1(this->RegionOfInterest[0], this->RegionOfInterest[2], zPos);
  TopLineSource->SetPoint2(this->RegionOfInterest[1], this->RegionOfInterest[2], zPos);
  RightLineSource->SetPoint1(this->RegionOfInterest[1], this->RegionOfInterest[2], zPos);
  RightLineSource->SetPoint2(this->RegionOfInterest[1], this->RegionOfInterest[3], zPos);
  BottomLineSource->SetPoint1(this->RegionOfInterest[0], this->RegionOfInterest[3], zPos);
  BottomLineSource->SetPoint2(this->RegionOfInterest[1], this->RegionOfInterest[3], zPos);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::InitializeROIVisualization()
{
  LOG_TRACE("vtkImageVisualizer::InitializeROIVisualization");

  vtkSmartPointer<vtkAssembly> assembly = vtkSmartPointer<vtkAssembly>::New();
  this->SetROIActorAssembly(assembly);

  vtkSmartPointer<vtkActor> leftLineActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> leftLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->LeftLineSource = vtkLineSource::New();
  leftLineActor->GetProperty()->SetColor(ROI_COLOR);
  leftLineMapper->SetInputConnection(this->LeftLineSource->GetOutputPort());
  leftLineActor->SetMapper(leftLineMapper);
  this->ROIActorAssembly->AddPart(leftLineActor);

  vtkSmartPointer<vtkActor> topLineActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> topLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->TopLineSource = vtkLineSource::New();
  topLineActor->GetProperty()->SetColor(ROI_COLOR);
  topLineMapper->SetInputConnection(this->TopLineSource->GetOutputPort());
  topLineActor->SetMapper(topLineMapper);
  this->ROIActorAssembly->AddPart(topLineActor);

  vtkSmartPointer<vtkActor> rightLineActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> rightLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->RightLineSource = vtkLineSource::New();
  rightLineActor->GetProperty()->SetColor(ROI_COLOR);
  rightLineMapper->SetInputConnection(this->RightLineSource->GetOutputPort());
  rightLineActor->SetMapper(rightLineMapper);
  this->ROIActorAssembly->AddPart(rightLineActor);

  vtkSmartPointer<vtkActor> bottomLineActor = vtkSmartPointer<vtkActor>::New();
  vtkSmartPointer<vtkPolyDataMapper> bottomLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->BottomLineSource = vtkLineSource::New();
  bottomLineActor->GetProperty()->SetColor(ROI_COLOR);
  bottomLineMapper->SetInputConnection(this->BottomLineSource->GetOutputPort());
  bottomLineActor->SetMapper(bottomLineMapper);
  this->ROIActorAssembly->AddPart(bottomLineActor);

  this->CanvasRenderer->AddActor(this->ROIActorAssembly);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::EnableROI( bool aEnable )
{
  LOG_TRACE("vtkImageVisualizer::EnableROI");

  if (aEnable) 
  {
    if( this->RegionOfInterest[0] == -1  || this->RegionOfInterest[1] == -1  || this->RegionOfInterest[2] == -1  || this->RegionOfInterest[3] == -1)
    {
      LOG_WARNING("Valid ROI is not defined. Check configuration or define valid ROI (Segmentation element, ClipRectangleOrigin and ClipRectangleSize attributes)");
      return PLUS_FAIL;
    }
    this->ROIActorAssembly->VisibilityOn();
  } 
  else 
  {
    this->ROIActorAssembly->VisibilityOff();
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::EnableWireLabels( bool aEnable )
{
  LOG_TRACE("vtkImageVisualizer::EnableWireLabels");

  for( std::vector<vtkTextActor3D*>::iterator it = this->WireActors.begin(); it != this->WireActors.end(); ++it )
  {
    if (aEnable) 
    {
      (*it)->VisibilityOn();
    } 
    else 
    {
      (*it)->VisibilityOff();
    }
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::InitializeWireLabelVisualization(vtkXMLDataElement* aConfig)
{
  LOG_TRACE("vtkImageVisualizer::InitializeWireLabelVisualization");

  this->ClearWireLabelVisualization();

  // Load phantom definition
  vtkXMLDataElement* phantomDefinition = aConfig->FindNestedElementWithName("PhantomDefinition");
  if (phantomDefinition == NULL)
  {
    LOG_WARNING("No phantom definition is found in the XML tree!");
  }
  else
  {
    // Load geometry
    vtkXMLDataElement* geometry = phantomDefinition->FindNestedElementWithName("Geometry"); 
    if (geometry == NULL) 
    {
      LOG_ERROR("Phantom geometry information not found!");
      return PLUS_FAIL;
    } 
    else 
    {
      // Finding of Patterns and extracting the endpoints
      int numberOfGeometryChildren = geometry->GetNumberOfNestedElements();
      for (int i=0; i<numberOfGeometryChildren; ++i) 
      {
        vtkXMLDataElement* patternElement = geometry->GetNestedElement(i);

        if ((patternElement == NULL) || (STRCASECMP("Pattern", patternElement->GetName()))) 
        {
          continue;
        }

        int numberOfWires = patternElement->GetNumberOfNestedElements();

        if ((numberOfWires != 3) && !(STRCASECMP("NWire", patternElement->GetAttribute("Type")))) 
        {
          LOG_WARNING("NWire contains unexpected number of wires - skipped");
          return PLUS_FAIL;
        }

        for (int j=0; j<numberOfWires; ++j) 
        {
          vtkXMLDataElement* wireElement = patternElement->GetNestedElement(j);

          if (wireElement == NULL) 
          {
            LOG_WARNING("Invalid Wire description in Pattern - skipped");
            return PLUS_FAIL;
          }

          const char* wireName =  wireElement->GetAttribute("Name"); 
          if ( wireName == NULL )
          {
            LOG_ERROR("Wire with no name in definition.");
            return PLUS_FAIL;
          }

          // Since the internal orientation is always MF, display the indicators for MF in all cases
          vtkSmartPointer<vtkTextActor3D> textActor = vtkSmartPointer<vtkTextActor3D>::New();
          textActor->GetTextProperty()->SetColor(RESULT_SPHERE_COLOR);
          textActor->GetTextProperty()->SetFontFamilyToArial();
          textActor->GetTextProperty()->SetFontSize(16);
          textActor->GetTextProperty()->SetJustificationToLeft();
          textActor->GetTextProperty()->SetVerticalJustificationToTop();
          textActor->GetTextProperty()->BoldOn(); 
          textActor->SetInput(wireName);
          textActor->RotateWXYZ(180.0, 1.0, 0.0, 0.0);
          this->WireActors.push_back(textActor);
          textActor->SetPosition(0.0, 0.0, -1.0);
          this->AddScreenAlignedProp(textActor);
        }
      }
    }
  }

  this->EnableWireLabels(false);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::SetWireLabelPositions( vtkPoints* aPointList )
{
  LOG_TRACE("vtkImageVisualizer::SetWireLabelPositions");

  if( aPointList == NULL || aPointList->GetNumberOfPoints() == 0)
  {
    this->EnableWireLabels(false);
    return PLUS_SUCCESS;
  }

  if( aPointList->GetNumberOfPoints() != this->WireActors.size() )
  {
    this->EnableWireLabels(false);
    LOG_ERROR("Number of candidates does not match the number of wire labels.");
    return PLUS_FAIL;
  }

  for( int i = 0; i < aPointList->GetNumberOfPoints(); ++i )
  {
    double* coords = aPointList->GetPoint(i);
    vtkTextActor3D* actor = this->WireActors.at(i);
    actor->SetPosition(coords[0] - 10.0, coords[1] - 10.0, actor->GetPosition()[2]);

    // Since we would like the screen aligned prop system to manage the orientation of these for us,
    // We must update the "original position" of the prop so that the system can use the latest position of the
    // wire labels
    int j = 0;
    vtkProp3D *prop=NULL;
    vtkCollectionSimpleIterator pit;
    for ( this->ScreenAlignedProps->InitTraversal(pit);  prop = this->ScreenAlignedProps->GetNextProp3D(pit); ++j)
    {
      vtkTextActor3D* ptr = dynamic_cast<vtkTextActor3D*>(prop);
      if( ptr != NULL && ptr == actor )
      {
        std::vector<double> origCoords = ScreenAlignedPropOriginalPosition[j];
        origCoords[0] = coords[0];
        origCoords[1] = coords[1];
        ScreenAlignedPropOriginalPosition[j] = origCoords;
        break;
      }
    }
  }
  this->EnableWireLabels(true);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::Reset()
{
  return this->ClearWireLabelVisualization();
}

//-----------------------------------------------------------------------------

PlusStatus vtkImageVisualizer::ClearWireLabelVisualization()
{
  for( std::vector<vtkTextActor3D*>::iterator it = this->WireActors.begin(); it != this->WireActors.end(); ++it )
  {
    this->RemoveScreenAlignedProp(*it);
  }
  this->WireActors.clear();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
void vtkImageVisualizer::SetChannel(vtkPlusChannel *channel)
{
  SetSelectedChannel(channel);

  if( this->SelectedChannel != NULL && this->SelectedChannel->GetBrightnessOutput() != NULL )
  {
    this->SetInputData(this->SelectedChannel->GetBrightnessOutput());
    this->ImageActor->VisibilityOn();
  }
  else
  {
    LOG_DEBUG("No video in the selected channel. Hiding image visualization.");
    this->ImageActor->VisibilityOff();
  }
}

//-----------------------------------------------------------------------------
PlusStatus vtkImageVisualizer::SetSliceNumber(int number)
{
  if( number >= this->ImageMapper->GetSliceNumberMinValue() && number <= this->ImageMapper->GetSliceNumberMaxValue() )
  {
    this->ImageMapper->SetSliceNumber(number);
    return PLUS_SUCCESS;
  }
  return PLUS_FAIL;
}
