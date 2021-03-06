/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#ifndef __vtk3DObjectVisualizer_h
#define __vtk3DObjectVisualizer_h

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkDisplayableObject.h"
#include "vtkGlyph3D.h"
#include "vtkImageActor.h"
#include "vtkObject.h"
#include "vtkPlusChannel.h"
#include "vtkPolyData.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkTransformRepository.h"

//-----------------------------------------------------------------------------

class vtkImageSliceMapper;

/*! \class vtk3DObjectVisualizer 
 * \brief Class that manages the displaying of a 3D object visualization in a QT canvas element
 * \ingroup PlusAppCommonWidgets
 */
class vtk3DObjectVisualizer : public vtkObject
{
public:
  /*!
  * New
  */
  static vtk3DObjectVisualizer *New();

  /*!
  * Return a displayable object
  * \param aModelId Model ID of the object to return
  */
  vtkDisplayableObject* GetObjectById( const char* aModelId );

  /*!
  * Return a displayable object
  * \param aModelId Model ID of the object to return
  */
  PlusStatus AddObject( vtkDisplayableObject* displayableObject );


  /*! Clear displayable object vector */
  PlusStatus ClearDisplayableObjects();

  /*!
  * Show or hide all displayable objects
  * \param aOn Show if true, else hide
  */
  PlusStatus ShowAllObjects(bool aOn);

  /* Return the actor of the volume actor */
  vtkActor* GetVolumeActor();

  /*!
  * Set the volume actor mapper
  * \param aContourMapper new mapper to use
  */
  PlusStatus SetVolumeMapper( vtkPolyDataMapper* aContourMapper );

  PlusStatus SetDataCollector();

  /*! Set the slice number
  * \param sliceNumber the slice number to display
  */
  PlusStatus SetSliceNumber(int number);

  /*!
  * Set the volume actor color
  * \param r red value
  * \param g green value
  * \param b blue value
  */
  PlusStatus SetVolumeColor( double r, double g, double b );

  /*!
  * Show or hide a displayable object
  * \param aModelId Model ID of the object to work on
  * \param aOn Show if true, else hide
  */
  PlusStatus ShowObjectById( const char* aModelId, bool aOn );

  /*!
  * Show or hide input points
  * \param aOn Show if true, else hide
  */
  PlusStatus ShowInput(bool aOn);

  /*!
  * Set the actor color of the input polydata
  * \param r g b
  */
  PlusStatus SetInputColor(double r, double g, double b);

  /*!
  * Show or hide result points
  * \param aOn Show if true, else hide
  */
  PlusStatus ShowResult(bool aOn);

  /*!
  * Hide all tools, other models and the image from main canvas
  */
  PlusStatus HideAll();

  /*!
  * Read the active configuration file to create displayable objects
  */
  PlusStatus ReadConfiguration(vtkXMLDataElement* aXMLElement);

  /*!
  * Update the displayable objects
  */
  PlusStatus Update();

  // Set/Get macros for member variables
  vtkGetObjectMacro(CanvasRenderer, vtkRenderer);
  vtkGetObjectMacro(ImageActor, vtkImageActor);
  vtkSetObjectMacro(CanvasRenderer, vtkRenderer);
  vtkGetStringMacro(WorldCoordinateFrame);
  vtkSetStringMacro(WorldCoordinateFrame);
  vtkGetStringMacro(VolumeID);  

  void SetInputPolyData(vtkPolyData* aPolyData);
  void SetResultPolyData(vtkPolyData* aPolyData);

  PlusStatus SetChannel(vtkPlusChannel* channel);

  vtkSetObjectMacro(TransformRepository, vtkTransformRepository);

protected:
  vtkSetObjectMacro(ImageActor, vtkImageActor);
  vtkSetObjectMacro(InputActor, vtkActor);
  vtkSetObjectMacro(ResultActor, vtkActor);
  vtkSetObjectMacro(ResultGlyph, vtkGlyph3D);
  vtkSetObjectMacro(InputGlyph, vtkGlyph3D);
  vtkGetObjectMacro(ResultGlyph, vtkGlyph3D);
  vtkGetObjectMacro(InputGlyph, vtkGlyph3D);
  vtkSetStringMacro(VolumeID);
  vtkSetObjectMacro(SelectedChannel, vtkPlusChannel);

protected:
  /*!
  * Constructor
  */
  vtk3DObjectVisualizer();

  /*!
  * Destructor
  */
  virtual ~vtk3DObjectVisualizer();

protected:

  /*! List of displayable objects */
  std::vector<vtkDisplayableObject*> DisplayableObjects;

  /*! Renderer for the canvas */
  vtkRenderer* CanvasRenderer; 

  /*! Canvas image actor */
  vtkImageActor* ImageActor;

  /*! Actor for displaying the input points in 3D */
  vtkActor* InputActor;

  /*! Slice mapper to enable slice selection */
  vtkImageSliceMapper* ImageMapper;

  /*! Glyph producer for result */
  vtkGlyph3D* InputGlyph;

  /*! Actor for displaying the result points (eg. stylus tip, segmented points) */
  vtkActor* ResultActor;

  /*! Glyph producer for result */
  vtkGlyph3D* ResultGlyph;

  /*! Name of the rendering world coordinate frame */
  char* WorldCoordinateFrame;

  /*! Name of the volume object ID */
  char* VolumeID;

  /*! Reference to Transform repository that stores and handles all transforms */
  vtkTransformRepository* TransformRepository;

  /*! Channel to visualize */
  vtkPlusChannel* SelectedChannel;
};

#endif  //__vtk3DObjectVisualizer_h