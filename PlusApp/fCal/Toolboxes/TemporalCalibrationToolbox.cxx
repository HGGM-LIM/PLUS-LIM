/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "TemporalCalibrationToolbox.h"
#include "TrackedFrame.h"
#include "fCalMainWindow.h"
#include "vtkPlusChannel.h"
#include "vtkPlusDataSource.h"
#include "vtkTemporalCalibrationAlgo.h"
#include "vtkTrackedFrameList.h"
#include "vtkVisualizationController.h"
#include <QFileDialog>
#include <QTimer>
#include <vtkChartXY.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkPlot.h>
#include <vtkRenderWindow.h>
#include <vtkTable.h>
#include <vtkXMLUtilities.h>

//-----------------------------------------------------------------------------
namespace
{
  const double INVALID_OFFSET = std::numeric_limits<double>::infinity();
  const double NEGLIGIBLE_OFFSET_DIFFERENCE_SEC=0.0001;
}

//-----------------------------------------------------------------------------

TemporalCalibrationToolbox::TemporalCalibrationToolbox(fCalMainWindow* aParentMainWindow, Qt::WindowFlags aFlags)
: AbstractToolbox(aParentMainWindow)
, QWidget(aParentMainWindow, aFlags)
, CancelRequest(false)
, LastRecordedFixedItemTimestamp(UNDEFINED_TIMESTAMP)
, m_FreeHandStartupDelaySec(5)
, m_StartupDelayRemainingTimeSec(0)
, m_StartupDelayTimer(NULL)
, LastRecordedMovingItemTimestamp(0.0)
, RecordingIntervalMs(200)
, TemporalCalibrationDurationSec(10)
, StartTimeSec(0.0)
, PreviousFixedOffset(INVALID_OFFSET)
, PreviousMovingOffset(INVALID_OFFSET)
, TemporalCalibrationPlotsWindow(NULL)
, UncalibratedPlotContextView(NULL)
, CalibratedPlotContextView(NULL)
, FixedChannel(NULL)
, FixedType(vtkTemporalCalibrationAlgo::FRAME_TYPE_NONE)
, MovingChannel(NULL)
, MovingType(vtkTemporalCalibrationAlgo::FRAME_TYPE_NONE)
, TemporalCalibrationAlgo(vtkTemporalCalibrationAlgo::New())
, RequestedFixedChannel("")
, RequestedMovingChannel("")
{
  ui.setupUi(this);

  // Create tracked frame lists
  TemporalCalibrationFixedData = vtkTrackedFrameList::New();
  TemporalCalibrationMovingData = vtkTrackedFrameList::New();

  // Set up timer to wait before acquisition
  m_StartupDelayTimer = new QTimer(this);

  // Create temporal calibration metric tables
  FixedPositionMetric = vtkTable::New();
  UncalibratedMovingPositionMetric = vtkTable::New();
  CalibratedMovingPositionMetric = vtkTable::New();

  // Connect events
  connect( ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( StartDelayTimer() ) );
  connect( m_StartupDelayTimer, SIGNAL(timeout()),this , SLOT(DelayStartup()));
  connect( ui.pushButton_ShowPlots, SIGNAL( toggled(bool) ), this, SLOT( ShowPlotsToggled(bool) ) );


  connect( ui.comboBox_FixedChannelValue, SIGNAL( currentIndexChanged(int) ), this, SLOT( FixedSignalChanged(int) ) );
  connect( ui.comboBox_MovingChannelValue, SIGNAL( currentIndexChanged(int) ), this, SLOT( MovingSignalChanged(int) ) );

  connect( ui.comboBox_FixedSourceValue, SIGNAL( currentIndexChanged(int) ), this, SLOT( FixedSourceChanged(int) ) );
  connect( ui.comboBox_MovingSourceValue, SIGNAL( currentIndexChanged(int) ), this, SLOT( MovingSourceChanged(int) ) );
}

//-----------------------------------------------------------------------------

TemporalCalibrationToolbox::~TemporalCalibrationToolbox()
{
  DELETE_IF_NOT_NULL(TemporalCalibrationFixedData);
  DELETE_IF_NOT_NULL(TemporalCalibrationMovingData);
  DELETE_IF_NOT_NULL(FixedPositionMetric);
  DELETE_IF_NOT_NULL(UncalibratedMovingPositionMetric);
  DELETE_IF_NOT_NULL(CalibratedMovingPositionMetric);
  if( TemporalCalibrationPlotsWindow != NULL )
  {
    delete TemporalCalibrationPlotsWindow;
  }
  DELETE_IF_NOT_NULL(UncalibratedPlotContextView);
  DELETE_IF_NOT_NULL(CalibratedPlotContextView);
  DELETE_IF_NOT_NULL(TemporalCalibrationAlgo);
  if (m_StartupDelayTimer != NULL)
  {
    if(m_StartupDelayTimer->isActive())
    {
      m_StartupDelayTimer->stop();
    }
    delete m_StartupDelayTimer;
    m_StartupDelayTimer = NULL;
  }
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::OnActivated()
{
  LOG_TRACE("TemporalCalibrationToolbox::OnActivated");

  if (m_State == ToolboxState_Done)
  {
    SetDisplayAccordingToState();
    return;
  }

  if ( (m_ParentMainWindow->GetVisualizationController()->GetDataCollector() != NULL)
    && (m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetConnected()))
  {
    // Read temporal calibration configuration
    if (this->ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
    {
      LOG_ERROR("Reading temporal calibration configuration failed!");
      return;
    }

    // Retrieve channels from devices
    DeviceCollection aCollection;
    if( m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetDevices(aCollection) != PLUS_SUCCESS )
    {
      ui.label_InstructionsTemporal->setText(tr("Unable to retrieve devices! See log."));
      SetState(ToolboxState_Error);
      return;
    }
    ui.comboBox_FixedChannelValue->clear();
    ui.comboBox_MovingChannelValue->clear();
    ui.comboBox_FixedSourceValue->clear();
    ui.comboBox_MovingSourceValue->clear();

    int requestedFixedIndex(0);
    int requestedMovingIndex(0);
    int index(0);
    for( DeviceCollectionIterator it = aCollection.begin(); it != aCollection.end(); ++it )
    {
      if( !(*it)->IsVirtual() )
      {
        for( ChannelContainerIterator chanIt = (*it)->GetOutputChannelsStart(); chanIt != (*it)->GetOutputChannelsEnd(); ++chanIt )
        {
          if( !this->RequestedFixedChannel.empty() && STRCASECMP((*chanIt)->GetChannelId(), this->RequestedFixedChannel.c_str()) == 0 )
          {
            requestedFixedIndex = index;
          }
          if( !this->RequestedMovingChannel.empty() && STRCASECMP((*chanIt)->GetChannelId(), this->RequestedMovingChannel.c_str()) == 0 )
          {
            requestedMovingIndex = index;
          }
          ui.comboBox_FixedChannelValue->addItem(QString((*chanIt)->GetChannelId()));
          ui.comboBox_MovingChannelValue->addItem(QString((*chanIt)->GetChannelId()));
          ++index;
        }
      }
    }

    ui.comboBox_FixedChannelValue->setCurrentIndex(requestedFixedIndex);
    this->FixedSignalChanged(requestedFixedIndex);
    if( !this->RequestedFixedSource.empty() )
    {
      int index = ui.comboBox_FixedSourceValue->findText(QString(this->RequestedFixedSource.c_str()));
      if( index >= 0 )
      {
        ui.comboBox_FixedSourceValue->setCurrentIndex(index);
      }
      else
      {
        LOG_WARNING("Unable to find element \'" << this->RequestedFixedSource << "\' in fixed source list.");
      }
    }

    ui.comboBox_MovingChannelValue->setCurrentIndex(requestedMovingIndex);
    this->MovingSignalChanged(requestedMovingIndex);
    if( !this->RequestedMovingSource.empty() )
    {
      int index = ui.comboBox_MovingSourceValue->findText(QString(this->RequestedMovingSource.c_str()));
      if( index >= 0 )
      {
        ui.comboBox_MovingSourceValue->setCurrentIndex(index);
      }
      else
      {
        LOG_WARNING("Unable to find element \'" << this->RequestedMovingSource << "\' in movving source list.");
      }
    }

    // Set initialized if it was uninitialized
    if (m_State == ToolboxState_Uninitialized || m_State == ToolboxState_Error)
    {
      SetState(ToolboxState_Idle);
    }
  }
  else
  {
    SetState(ToolboxState_Uninitialized);
  }

  this->PreviousFixedOffset = INVALID_OFFSET;
  this->PreviousMovingOffset = INVALID_OFFSET;

  SetDisplayAccordingToState();
}

//-----------------------------------------------------------------------------

PlusStatus TemporalCalibrationToolbox::ReadConfiguration(vtkXMLDataElement* aConfig)
{
  LOG_TRACE("TemporalCalibrationToolbox::ReadConfiguration");

  if (aConfig == NULL)
  {
    LOG_ERROR("Unable to read configuration"); 
    return PLUS_FAIL; 
  }

  vtkXMLDataElement* fCalElement = aConfig->FindNestedElementWithName("fCal");

  if (fCalElement == NULL)
  {
    LOG_ERROR("Unable to find fCal element in XML tree!"); 
    return PLUS_FAIL;     
  }

  // Recording interval and processing time
  int recordingIntervalMs = 0; 
  if ( fCalElement->GetScalarAttribute("RecordingIntervalMs", recordingIntervalMs ) )
  {
    RecordingIntervalMs = recordingIntervalMs;
  }
  else
  {
    LOG_WARNING("Unable to read RecordingIntervalMs attribute from fCal element of the device set configuration, default value '" << RecordingIntervalMs << "' will be used");
  }

  // Duration of temporal calibration
  int temporalCalibrationDurationSec = 0; 
  if ( fCalElement->GetScalarAttribute("TemporalCalibrationDurationSec", temporalCalibrationDurationSec ) )
  {
    TemporalCalibrationDurationSec = temporalCalibrationDurationSec;
  }
  else
  {
    LOG_WARNING("Unable to read TemporalCalibrationDurationSec attribute from fCal element of the device set configuration, default value '" << TemporalCalibrationDurationSec << "' will be used");
  }

  if( this->TemporalCalibrationAlgo->ReadConfiguration(aConfig) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to configure temporal calibration algorithm.");
    this->SetState(ToolboxState_Error);
  }

  if( fCalElement->GetAttribute("FixedChannelId") != NULL )
  {
    this->RequestedFixedChannel = fCalElement->GetAttribute("FixedChannelId");
  }
  if( fCalElement->GetAttribute("MovingChannelId") != NULL )
  {
    this->RequestedMovingChannel = fCalElement->GetAttribute("MovingChannelId");
  }
  if( fCalElement->GetAttribute("FixedSourceId") != NULL )
  {
    this->RequestedFixedSource = fCalElement->GetAttribute("FixedSourceId");
  }
  if( fCalElement->GetAttribute("MovingSourceId") != NULL )
  {
    this->RequestedMovingSource = fCalElement->GetAttribute("MovingSourceId");
  }

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, FreeHandStartupDelaySec , fCalElement);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::RefreshContent()
{
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::SetDisplayAccordingToState()
{
  LOG_TRACE("TemporalCalibrationToolbox::SetDisplayAccordingToState");

  // If connected
  if ( (m_ParentMainWindow->GetVisualizationController()->GetDataCollector() != NULL)
    && (m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetConnected()) )
  {
    // If the force show devices isn't enabled, set it to 2D
    if( !m_ParentMainWindow->IsForceShowDevicesEnabled() )
    {
      m_ParentMainWindow->GetVisualizationController()->HideAll();
      // 2D mode auto-turns back on the image
      m_ParentMainWindow->GetVisualizationController()->SetVisualizationMode(vtkVisualizationController::DISPLAY_MODE_2D);
    }

    // Enable or disable the image manipulation menu
    m_ParentMainWindow->SetImageManipulationMenuEnabled( m_ParentMainWindow->GetVisualizationController()->Is2DMode() );

    if (( m_State == ToolboxState_Idle || m_State == ToolboxState_Done )
      && this->MovingChannel != NULL && this->FixedChannel != NULL )
    {
      double timeOffsetDiff = this->MovingChannel->GetOwnerDevice()->GetLocalTimeOffsetSec()-this->FixedChannel->GetOwnerDevice()->GetLocalTimeOffsetSec();      
      // Update state message and fixed/moving offset labels
      std::string stateMsg;
      if (fabs(timeOffsetDiff)<NEGLIGIBLE_OFFSET_DIFFERENCE_SEC)
      {
        stateMsg += std::string("No time offset difference between ") + this->FixedChannel->GetOwnerDevice()->GetDeviceId() 
          + " and " + this->MovingChannel->GetOwnerDevice()->GetDeviceId() + "."
          + "\n\nProbably temporal calibration has not been performed yet.";
      }
      else if (timeOffsetDiff>0)
      {
        stateMsg += std::string(this->FixedChannel->GetOwnerDevice()->GetDeviceId()) + " lags compared to "
          + this->MovingChannel->GetOwnerDevice()->GetDeviceId()
          + " by " + GetTimeAsString(timeOffsetDiff)+".";
      }
      else
      {
        stateMsg += std::string(this->MovingChannel->GetOwnerDevice()->GetDeviceId()) + " lags compared to "
          + this->FixedChannel->GetOwnerDevice()->GetDeviceId()
          + " by " + GetTimeAsString(-timeOffsetDiff)+".";
      }
      if (this->PreviousFixedOffset != INVALID_OFFSET && this->PreviousMovingOffset != INVALID_OFFSET)
      {
        double previousTimeOffsetDiff = this->PreviousMovingOffset-this->PreviousFixedOffset;
        if (previousTimeOffsetDiff!=timeOffsetDiff) // don't print the difference if it's exactly the same (means that the new offset has not been computed)
        {
          stateMsg += "\n\nDifference compared to previous temporal calibration: "+GetTimeAsString(fabs(timeOffsetDiff-previousTimeOffsetDiff))+".";
        }
      }
      ui.label_State->setText(tr(stateMsg.c_str()));
      // Fixed label
      std::string fixedLabel = "Fixed (" + GetTimeAsString(this->FixedChannel->GetOwnerDevice()->GetLocalTimeOffsetSec())+"):";
      ui.label_FixedSeries->setText(tr(fixedLabel.c_str()));
      // Moving label
      std::string movingLabel = "Moving (" + GetTimeAsString(this->MovingChannel->GetOwnerDevice()->GetLocalTimeOffsetSec())+"):";
      ui.label_MovingSeries->setText(tr(movingLabel.c_str()));
    }
    else
    {
      ui.label_State->setText(tr(""));
    }
  }

  // Set widget states according to state
  if (m_State == ToolboxState_Uninitialized)
  {
    ui.label_InstructionsTemporal->setText(QString(""));
    ui.pushButton_StartCancelTemporal->setEnabled(false);
    ui.pushButton_ShowPlots->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString(""));
    m_ParentMainWindow->SetStatusBarProgress(-1);

    ui.comboBox_FixedChannelValue->setEnabled(false);
    ui.comboBox_MovingChannelValue->setEnabled(false);
    ui.comboBox_FixedSourceValue->setEnabled(false);
    ui.comboBox_MovingSourceValue->setEnabled(false);
  }
  else if (m_State == ToolboxState_Idle)
  {
    bool result = QString::compare(ui.comboBox_FixedChannelValue->currentText(), ui.comboBox_MovingChannelValue->currentText(), Qt::CaseInsensitive) == 0;
    result = result && QString::compare(ui.comboBox_FixedSourceValue->currentText(), ui.comboBox_MovingSourceValue->currentText(), Qt::CaseInsensitive) == 0;
    if( result )
    {
      ui.label_InstructionsTemporal->setText(tr("Please select different fixed and moving signal sources."));
    }
    else
    {
      ui.label_InstructionsTemporal->setText(tr("Move probe to vertical position in the water tank so that the bottom is visible and press Start"));
    }
    ui.pushButton_StartCancelTemporal->setEnabled(!result);
    ui.pushButton_ShowPlots->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString(""));
    m_ParentMainWindow->SetStatusBarProgress(-1);

    ui.comboBox_FixedChannelValue->setEnabled(true);
    ui.comboBox_MovingChannelValue->setEnabled(true);
    ui.comboBox_FixedSourceValue->setEnabled(ui.comboBox_FixedSourceValue->count() > 0 );
    ui.comboBox_MovingSourceValue->setEnabled(ui.comboBox_MovingSourceValue->count() > 0 );

    QApplication::restoreOverrideCursor();
  }
  else if (m_State == ToolboxState_StartupDelay)
  {
    ui.pushButton_StartCancelTemporal->setEnabled(true);
    ui.pushButton_StartCancelTemporal->setText("Cancel");

    ui.pushButton_ShowPlots->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString("Get ready for temporal calibration"));
    m_ParentMainWindow->SetStatusBarProgress(-1);

    ui.comboBox_FixedChannelValue->setEnabled(true);
    ui.comboBox_MovingChannelValue->setEnabled(true);
    ui.comboBox_FixedSourceValue->setEnabled(ui.comboBox_FixedSourceValue->count() > 0 );
    ui.comboBox_MovingSourceValue->setEnabled(ui.comboBox_MovingSourceValue->count() > 0 );
  }
  else if (m_State == ToolboxState_InProgress)
  {
    m_ParentMainWindow->SetStatusBarText(QString(" Acquiring and adding images to calibrator"));
    m_ParentMainWindow->SetStatusBarProgress(0);

    ui.label_InstructionsTemporal->setText(tr("Move probe up and down so that the tank bottom is visible with 2s period until the progress bar is filled"));
    ui.pushButton_StartCancelTemporal->setEnabled(true);

    ui.comboBox_FixedChannelValue->setEnabled(false);
    ui.comboBox_MovingChannelValue->setEnabled(false);
    ui.comboBox_FixedSourceValue->setEnabled(false);
    ui.comboBox_MovingSourceValue->setEnabled(false);
  }
  else if (m_State == ToolboxState_Done)
  {
    bool result = QString::compare(ui.comboBox_FixedChannelValue->currentText(), ui.comboBox_MovingChannelValue->currentText(), Qt::CaseInsensitive) == 0;
    result = result && QString::compare(ui.comboBox_FixedSourceValue->currentText(), ui.comboBox_MovingSourceValue->currentText(), Qt::CaseInsensitive) == 0;
    if( result )
    {
      ui.label_InstructionsTemporal->setText(tr("Please select different fixed and moving signal sources."));
    }
    else
    {
      ui.label_InstructionsTemporal->setText(tr("Temporal calibration is ready to save and its result plots can be viewed."));
    }
    ui.pushButton_ShowPlots->setEnabled(true);
    ui.pushButton_StartCancelTemporal->setEnabled(!result);

    m_ParentMainWindow->SetStatusBarText(QString(" Calibration done"));
    m_ParentMainWindow->SetStatusBarProgress(-1);

    ui.comboBox_FixedChannelValue->setEnabled(true);
    ui.comboBox_MovingChannelValue->setEnabled(true);
    ui.comboBox_FixedSourceValue->setEnabled(ui.comboBox_FixedSourceValue->count() > 0 );
    ui.comboBox_MovingSourceValue->setEnabled(ui.comboBox_MovingSourceValue->count() > 0 );

    QApplication::restoreOverrideCursor();
  }
  else if (m_State == ToolboxState_Error)
  {
    ui.label_InstructionsTemporal->setText(tr("Error occurred!"));
    ui.pushButton_StartCancelTemporal->setEnabled(false);
    ui.pushButton_ShowPlots->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString(""));
    m_ParentMainWindow->SetStatusBarProgress(-1);

    ui.comboBox_FixedChannelValue->setEnabled(false);
    ui.comboBox_MovingChannelValue->setEnabled(false);
    ui.comboBox_FixedSourceValue->setEnabled(false);
    ui.comboBox_MovingSourceValue->setEnabled(false);

    QApplication::restoreOverrideCursor();
  }
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::StartDelayTimer()
{
  LOG_INFO("Delay start up "<< m_StartupDelayRemainingTimeSec );

  disconnect(ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( StartDelayTimer() ));
  connect( ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( CancelCalibration() ) );
  ui.pushButton_StartCancelTemporal->setText(tr("Cancel"));

  if( m_State != ToolboxState_InProgress)
  {
    LOG_INFO("set current Delay start up"<<m_FreeHandStartupDelaySec);
    m_StartupDelayRemainingTimeSec=m_FreeHandStartupDelaySec;
    ui.label_InstructionsTemporal->setText(QString("Temporal calibration will start in %1").arg(m_StartupDelayRemainingTimeSec--));
    SetState(ToolboxState_StartupDelay);
    // Start timer every 1000 ms
    m_StartupDelayTimer->start(1000);
  }
}

//-----------------------------------------------------------------------------

void  TemporalCalibrationToolbox::DelayStartup()
{
  if(m_StartupDelayRemainingTimeSec>0)
  {
    ui.label_InstructionsTemporal->setText(QString("Temporal calibration will start in %1").arg(m_StartupDelayRemainingTimeSec--));
  }
  else
  {
    if(m_StartupDelayTimer->isActive())
    {
      m_StartupDelayTimer->stop();
    }
    StartCalibration();
  }
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::StartCalibration()
{
  LOG_INFO("Temporal calibration started");

  m_ParentMainWindow->SetToolboxesEnabled(false);

  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

  if( this->FixedChannel == NULL || this->MovingChannel == NULL )
  {
    if( m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetChannel(this->MovingChannel, 
      std::string(ui.comboBox_MovingChannelValue->currentText().toLatin1())) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve device " << std::string(ui.comboBox_MovingChannelValue->currentText().toLatin1()));
      return;
    }

    if( m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetChannel(this->FixedChannel, 
      std::string(ui.comboBox_FixedChannelValue->currentText().toLatin1())) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve device " << std::string(ui.comboBox_FixedChannelValue->currentText().toLatin1()));
      return;
    }
  }

  // Set validation transform names for tracked frame list
  // Set the local time offset to 0 before synchronization
  QString curFixedType = ui.comboBox_FixedSourceValue->itemData(ui.comboBox_FixedSourceValue->currentIndex()).toString();
  this->FixedType = vtkTemporalCalibrationAlgo::FRAME_TYPE_VIDEO;
  this->TemporalCalibrationFixedData->SetValidationRequirements(REQUIRE_UNIQUE_TIMESTAMP);
  if( QString::compare(curFixedType, QString("Video")) != 0 )
  {
    this->FixedType = vtkTemporalCalibrationAlgo::FRAME_TYPE_TRACKER;
    this->TemporalCalibrationFixedData->SetValidationRequirements(REQUIRE_UNIQUE_TIMESTAMP | REQUIRE_TRACKING_OK );
    this->FixedValidationTransformName.SetTransformName(std::string(ui.comboBox_FixedSourceValue->currentText().toLatin1()).c_str());
    this->TemporalCalibrationFixedData->SetFrameTransformNameForValidation(this->FixedValidationTransformName);
    LOG_DEBUG("Temporal calibration fixed signal: "<<this->FixedValidationTransformName.GetTransformName()<<" transform in channel "<<(this->FixedChannel->GetChannelId()?this->FixedChannel->GetChannelId():"(undefined)"));
  }
  else
  {
    LOG_DEBUG("Temporal calibration fixed signal: Video in channel "<<(this->FixedChannel->GetChannelId()?this->FixedChannel->GetChannelId():"(undefined)"));
  }
  this->PreviousFixedOffset = this->FixedChannel->GetOwnerDevice()->GetLocalTimeOffsetSec();

  QString curMovingType = ui.comboBox_MovingSourceValue->itemData(ui.comboBox_MovingSourceValue->currentIndex()).toString();
  this->MovingType = vtkTemporalCalibrationAlgo::FRAME_TYPE_VIDEO;
  this->TemporalCalibrationMovingData->SetValidationRequirements(REQUIRE_UNIQUE_TIMESTAMP);
  if( QString::compare(curMovingType, QString("Video")) != 0 )
  {
    this->MovingType = vtkTemporalCalibrationAlgo::FRAME_TYPE_TRACKER;
    this->TemporalCalibrationMovingData->SetValidationRequirements(REQUIRE_UNIQUE_TIMESTAMP | REQUIRE_TRACKING_OK );
    this->MovingValidationTransformName.SetTransformName(std::string(ui.comboBox_MovingSourceValue->currentText().toLatin1()).c_str());
    this->TemporalCalibrationMovingData->SetFrameTransformNameForValidation(this->MovingValidationTransformName);
    LOG_DEBUG("Temporal calibration moving signal: "<<this->MovingValidationTransformName.GetTransformName()<<" transform in channel "<<(this->MovingChannel->GetChannelId()?this->MovingChannel->GetChannelId():"(undefined)"));
  }
  else
  {
    LOG_DEBUG("Temporal calibration moving signal: Video in channel "<<(this->MovingChannel->GetChannelId()?this->MovingChannel->GetChannelId():"(undefined)"));
  }
  this->PreviousMovingOffset = this->MovingChannel->GetOwnerDevice()->GetLocalTimeOffsetSec();

  TemporalCalibrationFixedData->Clear();
  TemporalCalibrationMovingData->Clear();

  double currentTimeSec = vtkAccurateTimer::GetSystemTime();
  LastRecordedFixedItemTimestamp = UNDEFINED_TIMESTAMP; // means start from latest
  LastRecordedMovingItemTimestamp = UNDEFINED_TIMESTAMP; // means start from latest

  StartTimeSec = vtkAccurateTimer::GetSystemTime();
  CancelRequest = false;

  SetState(ToolboxState_InProgress);

  // Start calibration and compute results on success
  DoCalibration();
}

//-----------------------------------------------------------------------------
void TemporalCalibrationToolbox::ComputeCalibrationResults()
{
  // Create dialog
  QDialog* temporalCalibrationDialog = new QDialog(this, Qt::Dialog);
  temporalCalibrationDialog->setMinimumSize(QSize(360,80));
  temporalCalibrationDialog->setWindowTitle(tr("fCal"));
  temporalCalibrationDialog->setStyleSheet("QDialog { background-color: rgb(224, 224, 224); }");

  QLabel* temporalCalibrationLabel = new QLabel(QString("Computing temporal calibration, please wait..."), temporalCalibrationDialog);
  temporalCalibrationLabel->setFont(QFont("SansSerif", 16));

  QHBoxLayout* layout = new QHBoxLayout();
  layout->addWidget(temporalCalibrationLabel);

  temporalCalibrationDialog->setLayout(layout);
  temporalCalibrationDialog->show();

  ui.label_InstructionsTemporal->setText(tr("Please wait until computing temporal calibration is finished"));
  m_ParentMainWindow->SetStatusBarText(QString(" Computing temporal calibration"));

  QApplication::processEvents();

  this->TemporalCalibrationAlgo->SetFixedFrames(TemporalCalibrationFixedData, this->FixedType);
  if( this->FixedType == vtkTemporalCalibrationAlgo::FRAME_TYPE_TRACKER )
  {
    this->TemporalCalibrationAlgo->SetFixedProbeToReferenceTransformName(std::string(ui.comboBox_FixedSourceValue->currentText().toLatin1()));
  }
  this->TemporalCalibrationAlgo->SetMovingFrames(TemporalCalibrationMovingData, this->MovingType);
  if( this->MovingType == vtkTemporalCalibrationAlgo::FRAME_TYPE_TRACKER )
  {
    this->TemporalCalibrationAlgo->SetMovingProbeToReferenceTransformName(std::string(ui.comboBox_MovingSourceValue->currentText().toLatin1()));
  }

  this->TemporalCalibrationAlgo->SetSamplingResolutionSec(0.001);

  //  Calculate the time-offset
  vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR error = vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NONE;
  std::string errorStr;
  std::ostringstream strs;
  if (this->TemporalCalibrationAlgo->Update(error) != PLUS_SUCCESS)
  {
    switch (error)
    {
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_RESULT_ABOVE_THRESHOLD: 
      double correlation;
      this->TemporalCalibrationAlgo->GetBestCorrelation(correlation);

      strs << "Result above threshold. " << correlation;
      errorStr = strs.str();
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_INVALID_TRANSFORM_NAME:
      errorStr = "Invalid transform name.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NO_TIMESTAMPS:
      errorStr = "No timestamps on data.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_UNABLE_NORMALIZE_METRIC:
      errorStr = "Unable to normalize the data.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_CORRELATION_RESULT_EMPTY:
      errorStr = "Correlation list empty. Unable to perform analysis on data.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NO_VIDEO_DATA:
      errorStr = "Missing video data.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NOT_MF_ORIENTATION:
      errorStr = "Data not in MF orientation.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NOT_ENOUGH_FIXED_FRAMES:
      errorStr = "Not enough frames in fixed signal.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NO_FRAMES_IN_ULTRASOUND_DATA:
      errorStr = "No frames in ultrasound data.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_SAMPLING_RESOLUTION_TOO_SMALL:
      errorStr = "Sampling resolution too small.";
      break;
    case vtkTemporalCalibrationAlgo::TEMPORAL_CALIBRATION_ERROR_NONE:
      break;
    }

    LOG_ERROR("Cannot determine tracker lag, temporal calibration failed! Error: " << errorStr);
    CancelCalibration();

    QPalette palette;
    palette.setColor(ui.label_State->foregroundRole(), QColor::fromRgb(255, 0, 0));
    ui.label_State->setPalette(palette);
    ui.label_State->setText( QString(errorStr.c_str()) );

    temporalCalibrationDialog->done(0);
    temporalCalibrationDialog->hide();
    delete temporalCalibrationDialog;

    return;
  }

  // Get result
  double movingLagSec = 0;
  if (this->TemporalCalibrationAlgo->GetMovingLagSec(movingLagSec)!=PLUS_SUCCESS)
  {
    LOG_ERROR("Cannot determine time lag, temporal calibration failed");
    CancelCalibration();

    temporalCalibrationDialog->done(0);
    temporalCalibrationDialog->hide();
    delete temporalCalibrationDialog;

    return;
  }

  LOG_INFO("Temporal calibration result: moving stream lags by " << movingLagSec << "s");

  if( this->MovingChannel )
  {
    this->MovingChannel->GetOwnerDevice()->SetLocalTimeOffsetSec(this->MovingChannel->GetOwnerDevice()->GetLocalTimeOffsetSec()-movingLagSec);
  }

  // Update state message
  ui.label_State->setText(tr("Current moving time offset: %1 s").arg(movingLagSec));

  // Save metric tables
  this->TemporalCalibrationAlgo->GetFixedPositionSignal(this->FixedPositionMetric);
  this->FixedPositionMetric->GetColumn(0)->SetName("Time [s]");
  this->FixedPositionMetric->GetColumn(1)->SetName("Fixed signal");
  this->TemporalCalibrationAlgo->GetUncalibratedMovingPositionSignal(this->UncalibratedMovingPositionMetric);
  this->UncalibratedMovingPositionMetric->GetColumn(0)->SetName("Time [s]");
  this->UncalibratedMovingPositionMetric->GetColumn(1)->SetName("Moving signal before calibration");
  this->TemporalCalibrationAlgo->GetCalibratedMovingPositionSignal(this->CalibratedMovingPositionMetric);
  this->CalibratedMovingPositionMetric->GetColumn(0)->SetName("Time [s]");
  this->CalibratedMovingPositionMetric->GetColumn(1)->SetName("Moving signal after calibration");

  TemporalCalibrationFixedData->Clear();
  TemporalCalibrationMovingData->Clear();

  SetState(ToolboxState_Done);

  disconnect( ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( CancelCalibration() ) );
  connect(ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( StartDelayTimer() ));
  ui.pushButton_StartCancelTemporal->setText(tr("Start"));

  m_ParentMainWindow->SetToolboxesEnabled(true);

  // Close dialog
  temporalCalibrationDialog->done(0);
  temporalCalibrationDialog->hide();
  delete temporalCalibrationDialog;
}


//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::DoCalibration()
{
  LOG_TRACE("TemporalCalibrationToolbox::DoCalibration");

  // Get current time
  double currentTimeSec = vtkAccurateTimer::GetSystemTime();

  if (currentTimeSec - StartTimeSec >= this->TemporalCalibrationDurationSec)
  {
    // Update progress
    m_ParentMainWindow->SetStatusBarProgress(100);

    // The prescribed data collection time is up
    ComputeCalibrationResults();
    return;
  }

  // Cancel if requested
  if (CancelRequest)
  {
    LOG_INFO("Calibration process cancelled by the user");
    CancelCalibration();
    return;
  }

  int numberOfFixedFramesBeforeRecording = TemporalCalibrationFixedData->GetNumberOfTrackedFrames();
  int numberOfMovingFramesBeforeRecording = TemporalCalibrationMovingData->GetNumberOfTrackedFrames();

  if( this->FixedChannel != NULL )
  {
    if( this->FixedChannel->GetTrackedFrameList(this->LastRecordedFixedItemTimestamp, this->TemporalCalibrationFixedData, 50) != PLUS_SUCCESS )
    {
      LOG_ERROR("Failed to add data to fixed frame list.");
    }
  }
  else
  {
    LOG_ERROR("Unable to retrieve data for fixed channel. Aborting.");
    CancelCalibration();
    return;
  }
  if( this->MovingChannel != NULL )
  {
    if( this->MovingChannel->GetTrackedFrameList(this->LastRecordedMovingItemTimestamp, this->TemporalCalibrationMovingData, 50) != PLUS_SUCCESS )
    {
      LOG_ERROR("Failed to add data to moving frame list.");
    }
  }
  else
  {
    LOG_ERROR("Unable to retrieve data for moving channel. Aborting.");
    CancelCalibration();
    return;
  }

  // Update progress
  int progressPercent = (int)((currentTimeSec - StartTimeSec) / TemporalCalibrationDurationSec * 100.0);
  m_ParentMainWindow->SetStatusBarProgress(progressPercent);
  LOG_DEBUG("Number of tracked frames in the calibration dataset: Fixed: " << std::setw(3) << numberOfFixedFramesBeforeRecording << " => " << TemporalCalibrationFixedData->GetNumberOfTrackedFrames() << "; Moving: " << numberOfMovingFramesBeforeRecording << " => " << TemporalCalibrationMovingData->GetNumberOfTrackedFrames());

  QTimer::singleShot(RecordingIntervalMs, this, SLOT(DoCalibration())); 
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::CancelCalibration()
{
  LOG_INFO("Temporal calibration cancelled.");

  disconnect( ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( CancelCalibration() ) );
  connect(ui.pushButton_StartCancelTemporal, SIGNAL( clicked() ), this, SLOT( StartDelayTimer() ));
  ui.pushButton_StartCancelTemporal->setText(tr("Start"));

  if(m_State==ToolboxState_StartupDelay)
  {
    if(m_StartupDelayTimer->isActive())
    {
      m_StartupDelayTimer->stop();
    }
    LOG_TRACE("TemporalCalibrationToolbox::CancelCalibration before calibration delay timer finished");   }
  else
  {
    CancelRequest = true;

    this->PreviousFixedOffset = INVALID_OFFSET;
    this->PreviousMovingOffset = INVALID_OFFSET;

    this->FixedChannel = NULL;
    this->MovingChannel = NULL;
    this->FixedType = vtkTemporalCalibrationAlgo::FRAME_TYPE_NONE;
    this->MovingType = vtkTemporalCalibrationAlgo::FRAME_TYPE_NONE;
    this->FixedValidationTransformName.Clear();
    this->MovingValidationTransformName.Clear();

    m_ParentMainWindow->SetToolboxesEnabled(true);
  }

  SetState(ToolboxState_Idle);
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::ShowPlotsToggled(bool aOn)
{
  // Delete objects if toggled off, make sure they are deleted when toggled on
  if (TemporalCalibrationPlotsWindow != NULL)
  {
    delete TemporalCalibrationPlotsWindow;
    TemporalCalibrationPlotsWindow = NULL;
  } 

  if (UncalibratedPlotContextView != NULL)
  {
    UncalibratedPlotContextView->Delete();
    UncalibratedPlotContextView = NULL;
  } 

  if (CalibratedPlotContextView != NULL)
  {
    CalibratedPlotContextView->Delete();
    CalibratedPlotContextView = NULL;
  } 

  if (aOn)
  {
    // Create window and layout
    TemporalCalibrationPlotsWindow = new QWidget(this, Qt::Tool);
    TemporalCalibrationPlotsWindow->setMinimumSize(QSize(800, 600));
    TemporalCalibrationPlotsWindow->setWindowTitle(tr("Temporal calibration report"));
    TemporalCalibrationPlotsWindow->setStyleSheet("QWidget { background-color: rgb(255, 255, 255); }");

    // Install event filter that is called on closing the window
    TemporalCalibrationPlotsWindow->installEventFilter(this);

    QGridLayout* gridPlotWindow = new QGridLayout(TemporalCalibrationPlotsWindow);
    gridPlotWindow->setMargin(0);
    gridPlotWindow->setSpacing(4);

    // Uncalibrated chart view
    QVTKWidget* uncalibratedPlotVtkWidget = new QVTKWidget(TemporalCalibrationPlotsWindow);

    UncalibratedPlotContextView = vtkContextView::New();
    UncalibratedPlotContextView->GetRenderer()->SetBackground(1.0, 1.0, 1.0);

    vtkSmartPointer<vtkChartXY> uncalibratedChart = vtkSmartPointer<vtkChartXY>::New();
    uncalibratedChart->Register(UncalibratedPlotContextView);

    vtkPlot *uncalibratedTrackerMetricLine = uncalibratedChart->AddPlot(vtkChart::LINE);
    uncalibratedTrackerMetricLine->SetInputData_vtk5compatible(UncalibratedMovingPositionMetric, 0, 1);
    //vtkVariantArray* array = vtkVariantArray::New();
    //m_UncalibratedTrackerPositionMetric->GetRow(13, array);
    //array->PrintSelf(std::cout, *(vtkIndent::New()));
    //std::cout << array->GetValue(0) << "::" << array->GetValue(1) << std::endl;
    uncalibratedTrackerMetricLine->SetColor(1,0,0);
    uncalibratedTrackerMetricLine->SetWidth(1.0);

    vtkPlot *videoPositionMetricLineU = uncalibratedChart->AddPlot(vtkChart::LINE);
    videoPositionMetricLineU->SetInputData_vtk5compatible(FixedPositionMetric, 0, 1);
    videoPositionMetricLineU->SetColor(0,0,1);
    videoPositionMetricLineU->SetWidth(1.0);

    uncalibratedChart->SetShowLegend(true);
    UncalibratedPlotContextView->GetScene()->AddItem(uncalibratedChart);

    uncalibratedPlotVtkWidget->GetRenderWindow()->AddRenderer(UncalibratedPlotContextView->GetRenderer());
    uncalibratedPlotVtkWidget->GetRenderWindow()->SetSize(800,600);

    gridPlotWindow->addWidget(uncalibratedPlotVtkWidget, 0, 0);

    // Calibrated chart view
    QVTKWidget* calibratedPlotVtkWidget = new QVTKWidget(TemporalCalibrationPlotsWindow);

    CalibratedPlotContextView = vtkContextView::New();
    CalibratedPlotContextView->GetRenderer()->SetBackground(1.0, 1.0, 1.0);

    vtkSmartPointer<vtkChartXY> calibratedChart = vtkSmartPointer<vtkChartXY>::New();
    calibratedChart->Register(CalibratedPlotContextView);

    vtkPlot *calibratedTrackerMetricLine = calibratedChart->AddPlot(vtkChart::LINE);
    calibratedTrackerMetricLine->SetInputData_vtk5compatible(CalibratedMovingPositionMetric, 0, 1);
    calibratedTrackerMetricLine->SetColor(0,1,0);
    calibratedTrackerMetricLine->SetWidth(1.0);

    vtkPlot *videoPositionMetricLineC = calibratedChart->AddPlot(vtkChart::LINE);
    videoPositionMetricLineC->SetInputData_vtk5compatible(FixedPositionMetric, 0, 1);
    videoPositionMetricLineC->SetColor(0,0,1);
    videoPositionMetricLineC->SetWidth(1.0);

    calibratedChart->SetShowLegend(true);
    CalibratedPlotContextView->GetScene()->AddItem(calibratedChart);

    calibratedPlotVtkWidget->GetRenderWindow()->AddRenderer(CalibratedPlotContextView->GetRenderer());
    calibratedPlotVtkWidget->GetRenderWindow()->SetSize(800,600);

    gridPlotWindow->addWidget(calibratedPlotVtkWidget, 1, 0);

    // Position and show window
    TemporalCalibrationPlotsWindow->setLayout(gridPlotWindow);
    TemporalCalibrationPlotsWindow->move( mapToGlobal( QPoint( ui.pushButton_ShowPlots->x() + ui.pushButton_ShowPlots->width(), 20 ) ) );
    TemporalCalibrationPlotsWindow->show();
  }
}

//-----------------------------------------------------------------------------

bool TemporalCalibrationToolbox::eventFilter(QObject *obj, QEvent *ev)
{
  if ( obj == TemporalCalibrationPlotsWindow )
  {
    if ( ev->type() == QEvent::Close )
    {
      ui.pushButton_ShowPlots->setChecked(false);
    }
    else
    {
      // Pass the event on to the parent class
      return QWidget::eventFilter( obj, ev );
    }
  }

  return true;
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::OnDeactivated()
{
  // The channels may get deleted (e.g., by a disconnect/connect), so make sure they are not reused
  this->FixedChannel = NULL;
  this->MovingChannel = NULL;
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::FixedSignalChanged( int newIndex )
{
  ui.comboBox_FixedSourceValue->clear();

  if( ui.comboBox_FixedChannelValue->currentIndex() != -1 )
  {
    if( m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetChannel(this->FixedChannel, 
      std::string(ui.comboBox_FixedChannelValue->currentText().toLatin1())) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve device " << std::string(ui.comboBox_FixedChannelValue->currentText().toLatin1()));
      return;
    }

    // Determine data types, populate
    vtkPlusDataSource* aSource(NULL);
    if( this->FixedChannel->GetVideoSource(aSource) == PLUS_SUCCESS )
    {
      QVariant strVar = QVariant::fromValue(QString("Video"));
      ui.comboBox_FixedSourceValue->addItem(aSource->GetSourceId(), strVar);
    }
    TrackedFrame frame;
    if( this->FixedChannel->GetTrackedFrame(&frame) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve tracked frame from channel: " << this->FixedChannel->GetChannelId() );
      return;
    }
    vtkTransformRepository* repo = vtkTransformRepository::New();
    repo->SetTransforms(frame);

    std::vector<PlusTransformName> nameList;
    frame.GetCustomFrameTransformNameList(nameList);
    std::vector<std::string> fromList;
    std::vector<std::string> toList;
    for (std::vector<PlusTransformName>::iterator it = nameList.begin(); it != nameList.end(); ++it )
    {
      if( std::find(fromList.begin(), fromList.end(), it->From()) == fromList.end() )
      {
        fromList.push_back(it->From());
        toList.push_back(it->From());
      }
      if( std::find(toList.begin(), toList.end(), it->To()) == toList.end() )
      {
        fromList.push_back(it->To());
        toList.push_back(it->To());
      }
    }

    // build exhaustive list of all possible transforms
    for (std::vector<std::string>::iterator fromIt = fromList.begin(); fromIt != fromList.end(); ++fromIt )
    {
      for (std::vector<std::string>::iterator toIt = toList.begin(); toIt != toList.end(); ++toIt )
      {
        if( fromIt->compare(*toIt) == 0 )
        {
          continue;
        }

        PlusTransformName trans;
        std::stringstream ss;
        ss << (*fromIt) << "To" << (*toIt);
        trans.SetTransformName(ss.str().c_str());

        if( trans.IsValid() && repo->IsExistingTransform(trans) )
        {
          QVariant strVar = QVariant::fromValue(QString("Transform"));
          ui.comboBox_FixedSourceValue->addItem(QString(ss.str().c_str()), strVar);
        }
      }
    }

    repo->Delete();
  }

  this->SetDisplayAccordingToState();
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::MovingSignalChanged( int newIndex )
{
  ui.comboBox_MovingSourceValue->clear();

  if( ui.comboBox_MovingChannelValue->currentIndex() != -1 )
  {
    if( m_ParentMainWindow->GetVisualizationController()->GetDataCollector()->GetChannel(this->MovingChannel, 
      std::string(ui.comboBox_MovingChannelValue->currentText().toLatin1())) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve device " << std::string(ui.comboBox_MovingChannelValue->currentText().toLatin1()));
      return;
    }

    // Determine data types, populate
    vtkPlusDataSource* aSource(NULL);
    if( this->MovingChannel->GetVideoSource(aSource) == PLUS_SUCCESS )
    {
      QVariant strVar = QVariant::fromValue(QString("Video"));
      ui.comboBox_MovingSourceValue->addItem(aSource->GetSourceId(), strVar);
    }
    TrackedFrame frame;
    if( this->MovingChannel->GetTrackedFrame(&frame) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to retrieve tracked frame from channel: " << this->MovingChannel->GetChannelId() );
      return;
    }
    vtkTransformRepository* repo = vtkTransformRepository::New();
    repo->SetTransforms(frame);

    std::vector<PlusTransformName> nameList;
    frame.GetCustomFrameTransformNameList(nameList);
    std::vector<std::string> fromList;
    std::vector<std::string> toList;
    for (std::vector<PlusTransformName>::iterator it = nameList.begin(); it != nameList.end(); ++it )
    {
      if( std::find(fromList.begin(), fromList.end(), it->From()) == fromList.end() )
      {
        fromList.push_back(it->From());
        toList.push_back(it->From());
      }
      if( std::find(toList.begin(), toList.end(), it->To()) == toList.end() )
      {
        fromList.push_back(it->To());
        toList.push_back(it->To());
      }
    }

    // build exhaustive list of all possible transforms
    for (std::vector<std::string>::iterator fromIt = fromList.begin(); fromIt != fromList.end(); ++fromIt )
    {
      for (std::vector<std::string>::iterator toIt = toList.begin(); toIt != toList.end(); ++toIt )
      {
        if( fromIt->compare(*toIt) == 0 )
        {
          continue;
        }

        PlusTransformName trans;
        std::stringstream ss;
        ss << (*fromIt) << "To" << (*toIt);
        trans.SetTransformName(ss.str().c_str());

        if( trans.IsValid() && repo->IsExistingTransform(trans) )
        {
          QVariant strVar = QVariant::fromValue(QString("Transform"));
          ui.comboBox_MovingSourceValue->addItem(QString(ss.str().c_str()), strVar);
        }
      }
    }

    repo->Delete();
  }

  this->SetDisplayAccordingToState();
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::FixedSourceChanged( int newIndex )
{
  this->SetDisplayAccordingToState();
}

//-----------------------------------------------------------------------------

void TemporalCalibrationToolbox::MovingSourceChanged( int newIndex )
{
  this->SetDisplayAccordingToState();
}

//-----------------------------------------------------------------------------

std::string TemporalCalibrationToolbox::GetTimeAsString(double timeSec)
{
  std::stringstream ss;
  ss << std::fixed << std::setprecision(1) << 1000.0*timeSec << "ms";
  return ss.str();
}
