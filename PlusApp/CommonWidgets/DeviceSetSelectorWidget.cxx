/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "DeviceSetSelectorWidget.h"

#include <QAction>
#include <QDesktopServices>
#include <QDomDocument>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>

#ifdef _WIN32
#include "Shellapi.h"
#else
#include <unistd.h>
#endif

enum DataItemRoles
{
  FileNameRole = Qt::UserRole+0,
  DescriptionRole = Qt::UserRole+1
};

//-----------------------------------------------------------------------------

DeviceSetSelectorWidget::DeviceSetSelectorWidget(QWidget* aParent)
  : QWidget(aParent)
  , m_ConnectionSuccessful(false)
  , m_EditMenu(NULL)
  , m_EditorSelectAction(NULL)
{
  ui.setupUi(this);

  ui.pushButton_EditConfiguration->setContextMenuPolicy(Qt::CustomContextMenu);

  connect( ui.pushButton_OpenConfigurationDirectory, SIGNAL( clicked() ), this, SLOT( OpenConfigurationDirectory() ) );
  connect( ui.pushButton_Connect, SIGNAL( clicked() ), this, SLOT( InvokeConnect() ) );
  connect( ui.pushButton_RefreshFolder, SIGNAL( clicked() ), this, SLOT( RefreshFolder() ) );
  connect( ui.pushButton_EditConfiguration, SIGNAL( clicked() ), this, SLOT( EditConfiguration() ) );
  connect( ui.comboBox_DeviceSet, SIGNAL( currentIndexChanged(int) ), this, SLOT( DeviceSetSelected(int) ) );
  connect( ui.pushButton_ResetTracker, SIGNAL( clicked() ), this, SLOT( ResetTrackerButtonClicked() ) );
  ui.pushButton_ResetTracker->setVisible(false);
  
  connect( ui.pushButton_EditConfiguration, SIGNAL( customContextMenuRequested( QPoint ) ), this, SLOT( ShowEditContextMenu(QPoint) ) );

  SetConfigurationDirectory(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationDirectory().c_str());
}

//-----------------------------------------------------------------------------

DeviceSetSelectorWidget::~DeviceSetSelectorWidget()
{
  if( this->m_EditMenu)
  {
    delete this->m_EditMenu;
    m_EditMenu = NULL;
  }
  if( this->m_EditorSelectAction)
  {
    delete this->m_EditorSelectAction;
    m_EditorSelectAction = NULL;
  }
}

//-----------------------------------------------------------------------------

PlusStatus DeviceSetSelectorWidget::SetConfigurationDirectory(QString aDirectory)
{
  LOG_TRACE("DeviceSetSelectorWidget::SetConfigurationDirectory(" << aDirectory.toLatin1().constData() << ")");

  // Try to parse up directory and set UI according to the result
  if (ParseDirectory(aDirectory))
  {
    ui.lineEdit_ConfigurationDirectory->setText(aDirectory);
    ui.lineEdit_ConfigurationDirectory->setToolTip(aDirectory);

    m_ConfigurationDirectory = aDirectory;

    return PLUS_SUCCESS;
  }
  else
  {
    ui.lineEdit_ConfigurationDirectory->setText(tr("Invalid configuration directory"));
    ui.lineEdit_ConfigurationDirectory->setToolTip("No valid configuration files in directory, please select another");

    ui.textEdit_Description->setTextColor(QColor(Qt::darkRed));
    ui.textEdit_Description->setText("Selected directory does not contain valid device set configuration files!\n\nPlease select another directory");

    return PLUS_FAIL;
  }
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::OpenConfigurationDirectory()
{
  LOG_TRACE("DeviceSetSelectorWidget::OpenConfigurationDirectoryClicked"); 

  // Directory open dialog for selecting configuration directory 
  QString dirName = QFileDialog::getExistingDirectory(NULL, QString( tr( "Open configuration directory" ) ), m_ConfigurationDirectory);
  if (dirName.isNull())
  {
    return;
  }

  if (SetConfigurationDirectory(dirName) == PLUS_SUCCESS)
  {
    // Save the selected directory to config object
    vtkPlusConfig::GetInstance()->SetDeviceSetConfigurationDirectory(dirName.toLatin1().constData());
    vtkPlusConfig::GetInstance()->SaveApplicationConfigurationToFile();
  }
  else
  {
    LOG_ERROR("Unable to open selected directory!");
  }
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::InvokeConnect()
{
  LOG_TRACE("DeviceSetSelectorWidget::InvokeConnect"); 

  if ( ui.comboBox_DeviceSet->currentIndex() < 0 )
  {
    // combo box is empty 
    return; 
  }

  ui.pushButton_Connect->setEnabled(false);
  QCoreApplication::processEvents();

  emit ConnectToDevicesByConfigFileInvoked(ui.comboBox_DeviceSet->itemData(ui.comboBox_DeviceSet->currentIndex(), FileNameRole).toString().toLatin1().constData());
}

//-----------------------------------------------------------------------------

std::string DeviceSetSelectorWidget::GetSelectedDeviceSetDescription()
{
  LOG_TRACE("DeviceSetSelectorWidget::GetSelectedDeviceSetDescription"); 

  return ui.comboBox_DeviceSet->itemData(ui.comboBox_DeviceSet->currentIndex(), DescriptionRole).toString().toLatin1().constData(); 
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::InvokeDisconnect()
{
  LOG_TRACE("DeviceSetSelectorWidget::InvokeDisconnect"); 

  RefreshFolder();

  ui.pushButton_Connect->setEnabled(false);
  QCoreApplication::processEvents();

  emit ConnectToDevicesByConfigFileInvoked("");
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::DeviceSetSelected(int aIndex)
{
  LOG_TRACE("DeviceSetSelectorWidget::DeviceSetSelected(" << aIndex << ")"); 

  if ((aIndex < 0) || (aIndex >= ui.comboBox_DeviceSet->count()))
  {
    return;
  }

  ui.textEdit_Description->setTextColor(QColor(Qt::black));

  ui.textEdit_Description->setText(ui.comboBox_DeviceSet->itemData(aIndex, DescriptionRole).toString());

  ui.comboBox_DeviceSet->setToolTip(ui.comboBox_DeviceSet->currentText() + "\n" + ui.comboBox_DeviceSet->itemData(aIndex, FileNameRole).toString());

  QString configurationFilePath = ui.comboBox_DeviceSet->itemData(ui.comboBox_DeviceSet->currentIndex(), FileNameRole).toString(); 

  emit DeviceSetSelected( configurationFilePath.toLatin1().constData() ); 
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::SetConnectionSuccessful(bool aConnectionSuccessful)
{
  LOG_TRACE("DeviceSetSelectorWidget::SetConnectionSuccessful(" << (aConnectionSuccessful?"true":"false") << ")"); 

  m_ConnectionSuccessful = aConnectionSuccessful;

  // If connect button has been pushed
  if ( !ui.pushButton_Connect->property("connected").toBool() )
  {
    if (m_ConnectionSuccessful)
    {
      ui.pushButton_Connect->setText(tr("Disconnect"));
      ui.comboBox_DeviceSet->setEnabled(false);

      ui.textEdit_Description->setTextColor(QColor(Qt::black));
      m_DescriptionPrefix = "Connection successful!";
      m_DescriptionBody = ui.comboBox_DeviceSet->itemData(ui.comboBox_DeviceSet->currentIndex(), DescriptionRole).toString();
      this->UpdateDescriptionText();

      // Change the function to be invoked on clicking on the now Disconnect button to InvokeDisconnect
      disconnect( ui.pushButton_Connect, SIGNAL( clicked() ), this, SLOT( InvokeConnect() ) );
      connect( ui.pushButton_Connect, SIGNAL( clicked() ), this, SLOT( InvokeDisconnect() ) );

      // Set last used device set config file
      vtkPlusConfig::GetInstance()->SetDeviceSetConfigurationFileName(ui.comboBox_DeviceSet->itemData(ui.comboBox_DeviceSet->currentIndex(), FileNameRole).toString().toLatin1().constData());

      ui.pushButton_Connect->setProperty("connected", QVariant(true));
    }
    else
    {
      ui.textEdit_Description->setTextColor(QColor(Qt::darkRed));
      m_DescriptionPrefix = "Connection failed!";
      m_DescriptionBody = "Please select another device set and try again!";
      this->UpdateDescriptionText();
    }
  }
  else
  { // If disconnect button has been pushed
    if ( !m_ConnectionSuccessful )
    {
      ui.pushButton_Connect->setProperty("connected", QVariant(false));
      ui.pushButton_Connect->setText(tr("Connect"));
      ui.comboBox_DeviceSet->setEnabled(true);

      ui.textEdit_Description->setTextColor(QColor(Qt::black));
      if( ui.comboBox_DeviceSet->currentIndex() >= 0 )
      {
        m_DescriptionPrefix = "";
        m_DescriptionBody = ui.comboBox_DeviceSet->itemData(ui.comboBox_DeviceSet->currentIndex(), DescriptionRole).toString();
        this->UpdateDescriptionText();
      }

      // Change the function to be invoked on clicking on the now Connect button to InvokeConnect
      disconnect( ui.pushButton_Connect, SIGNAL( clicked() ), this, SLOT( InvokeDisconnect() ) );
      connect( ui.pushButton_Connect, SIGNAL( clicked() ), this, SLOT( InvokeConnect() ) );
    }
    else
    {
      LOG_ERROR("Disconnect failed!");
    }
  }

  ui.pushButton_Connect->setEnabled(true);
}

//-----------------------------------------------------------------------------

bool DeviceSetSelectorWidget::GetConnectionSuccessful()
{
  LOG_TRACE("DeviceSetSelectorWidget::GetConnectionSuccessful"); 

  return m_ConnectionSuccessful;
}

//-----------------------------------------------------------------------------

PlusStatus DeviceSetSelectorWidget::ParseDirectory(QString aDirectory)
{
  LOG_TRACE("DeviceSetSelectorWidget::ParseDirectory(" << aDirectory.toLatin1().constData() << ")"); 

  QDir configDir(aDirectory);
  QStringList fileList(configDir.entryList());

  if (fileList.size() > 200)
  {
    if (QMessageBox::No == QMessageBox::question(this, tr("Many files in the directory"), tr("There are more than 200 files in the selected directory. Do you really want to continue parsing the files?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes))
    {
      return PLUS_FAIL;
    }
  }

  // Block signals before we add items
  ui.comboBox_DeviceSet->blockSignals(true); 
  ui.comboBox_DeviceSet->clear();

  QStringListIterator filesIterator(fileList);
  QHash<QString, int> deviceSetVersion;

  while (filesIterator.hasNext())
  {

    QString fileName = QDir::toNativeSeparators(QString(configDir.absoluteFilePath(filesIterator.next())));
    QString extension = fileName.mid(fileName.lastIndexOf("."));
    if( extension.compare(QString(".xml")) != 0 )
    {
      continue;
    }

    QFile file(fileName);
    QFileInfo fileInfo(fileName);
    QDomDocument doc;

    // If file is not readable then skip
    if (!file.open(QIODevice::ReadOnly))
    {
      continue;
    }

    // If parsing is successful then check the content and if data collection config file is found then add it to combo box
    if (doc.setContent(&file))
    {
      QDomElement docElem = doc.documentElement();

      // Check if the root element is PlusConfiguration and contains a DataCollection child
      if (! docElem.tagName().compare("PlusConfiguration"))
      {
        QDomNodeList list = docElem.elementsByTagName("DataCollection");

        if (list.count() > 0)
        { // If it has a DataCollection children then use the first one
          docElem = list.at(0).toElement();
        }
        else
        { // If it does not have a DataCollection then it cannot be used for connecting
          continue;
        }
      }
      else
      {
        continue;
      }

      // Add the name attribute to the first node named DeviceSet to the combo box
      QDomNodeList list(doc.elementsByTagName("DeviceSet"));
      if (list.size() <= 0)
      {
        continue;
      }

      QDomElement elem = list.at(0).toElement();

      QString name(elem.attribute("Name"));
      QString description(elem.attribute("Description"));
      if (name.isEmpty())
      {
        LOG_WARNING("Name field is empty in device set configuration file '" << fileName.toLatin1().constData() << "', it is not added to the list");
        continue;
      }

      // Check if the same name already exists, add a version number if it does.
      int foundIndex = ui.comboBox_DeviceSet->findText(name, Qt::MatchExactly);
      if (foundIndex > -1)
      {
        QHash<QString, int>::iterator deviceIt = deviceSetVersion.find(name);
        if(deviceIt == deviceSetVersion.end())
        {
          deviceSetVersion.insert(name,0);
          name.append(" [0]");
        }
        else
        {
          deviceIt.value()+=1;
          name.append(" [" + QString::number(deviceIt.value())+"]");
        }
      }

      ui.comboBox_DeviceSet->addItem(name, fileName);
      int currentIndex = ui.comboBox_DeviceSet->findText(name, Qt::MatchExactly);

      ui.comboBox_DeviceSet->setItemData(currentIndex, description, DescriptionRole); 

      // Add tooltip word wrapped rich text  
      name.prepend("<p>");
      name.append("</p> <p>"+fileInfo.fileName().toLatin1()+"</p> <p>"+description.toLatin1()+"</p>");
      ui.comboBox_DeviceSet->setItemData(currentIndex, name, Qt::ToolTipRole); 

    }
    else
    {
      LOG_WARNING("Unable to parse file '" << fileName.toLatin1().constData() << "' as an XML. It will not appear in the device set configuration file list!");
    }
  }

  // If no valid configuration files have been parsed then warn user
  if (ui.comboBox_DeviceSet->count() < 1)
  {
    LOG_ERROR("Selected directory ("<<aDirectory.toLatin1().constData()<<") does not contain valid device set configuration files!");
    return PLUS_FAIL;
  }

  // Set current index to default so that setting the last selected item raises the event even if it is the first item
  ui.comboBox_DeviceSet->setCurrentIndex(-1);

  ui.comboBox_DeviceSet->model()->sort(0);

  // Unblock signals after we add items
  ui.comboBox_DeviceSet->blockSignals(false); 

  // Restore the  saved selection
  int lastSelectedDeviceSetIndex = ui.comboBox_DeviceSet->findData( QDir::toNativeSeparators(QString(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationFileName().c_str())));
  ui.comboBox_DeviceSet->setCurrentIndex(lastSelectedDeviceSetIndex);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
void DeviceSetSelectorWidget::UpdateDescriptionText()
{
  QString text;
  if( !m_DescriptionPrefix.isEmpty() )
  {
    text += m_DescriptionPrefix + "\n\n";
  }

  text += m_DescriptionBody;

  if( !m_DescriptionSuffix.isEmpty() )
  {
    text += "\n\n" + m_DescriptionSuffix;
  }

  ui.textEdit_Description->setText(text);
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::RefreshFolder()
{
  LOG_TRACE("DeviceSetSelectorWidget::RefreshFolderClicked"); 

  if (ParseDirectory(QString(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationDirectory().c_str())) != PLUS_SUCCESS)
  {
    LOG_ERROR("Parsing up configuration files failed in: " << vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationDirectory());
  }
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::EditConfiguration()
{
  LOG_TRACE("DeviceSetSelectorWidget::EditConfiguration"); 

  QString configurationFilePath;
  int deviceSetIndex=ui.comboBox_DeviceSet->currentIndex();
  if (deviceSetIndex>=0)
  {
    configurationFilePath=ui.comboBox_DeviceSet->itemData(deviceSetIndex).toStringList().at(0);
  }
  else
  {
    LOG_ERROR("Cannot edit configuration file. No configuration is selected."); 
    return;
  }
  QString editorApplicationExecutable(vtkPlusConfig::GetInstance()->GetEditorApplicationExecutable().c_str());

#ifdef _WIN32
  if (!editorApplicationExecutable.isEmpty())
  {
    // TODO: instead of 1024 we should get the buffer length from the QString objects
    wchar_t wcharApplication[1024];
    wchar_t wcharFile[1024];

    QFileInfo fileInfo( QDir::toNativeSeparators( configurationFilePath ) );

    QString file = fileInfo.absoluteFilePath();
    int lenFile = file.toWCharArray( wcharFile );
    wcharFile[lenFile] = '\0';

    int lenApplication = editorApplicationExecutable.toWCharArray( wcharApplication );
    wcharApplication[lenApplication] = '\0';

    ShellExecuteW( 0, L"open", wcharApplication, wcharFile, NULL, SW_MAXIMIZE );
    return;
  }
#endif

  if (!QDesktopServices::openUrl(QUrl("file:///"+configurationFilePath, QUrl::TolerantMode)))
  {
    LOG_ERROR("Failed to open file in default application: "<<configurationFilePath.toLatin1().constData());
  }
}

//----------------------------------------------------------------------------
void DeviceSetSelectorWidget::ShowEditContextMenu(QPoint point)
{
  if( m_EditorSelectAction == NULL )
  {
    m_EditorSelectAction = new QAction(this);
    m_EditorSelectAction->setText("Select Editor");
    connect(m_EditorSelectAction, SIGNAL(triggered()), this, SLOT( SelectEditor() ));
  }

  if( m_EditMenu == NULL )
  {
    m_EditMenu = new QMenu(this);
    m_EditMenu->addAction(m_EditorSelectAction);
  }
  m_EditMenu->exec(ui.pushButton_EditConfiguration->mapToGlobal(point));
}

//-----------------------------------------------------------------------------
void DeviceSetSelectorWidget::SelectEditor()
{
  // File open dialog for selecting editor application
  QString filter = QString( tr( "Executables ( *.exe );;" ) );
  QString fileName = QFileDialog::getOpenFileName(NULL, QString( tr( "Select XML editor application" ) ), "", filter);
  if (fileName.isNull()) {
    return;
  }

  vtkPlusConfig::GetInstance()->SetEditorApplicationExecutable( std::string(fileName.toLatin1().constData()) );
  vtkPlusConfig::GetInstance()->SaveApplicationConfigurationToFile();
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::ResetTrackerButtonClicked()
{
  LOG_TRACE("DeviceSetSelectorWidget::ResetTrackerButtonClicked()");

  emit ResetTracker();
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::ShowResetTrackerButton( bool aValue )
{
  ui.pushButton_ResetTracker->setVisible(aValue);
}

//-----------------------------------------------------------------------------

void DeviceSetSelectorWidget::SetConnectButtonText(QString text)
{
  ui.pushButton_Connect->setText(text);
}

//----------------------------------------------------------------------------
void DeviceSetSelectorWidget::SetDescriptionSuffix(const QString& string)
{
  this->m_DescriptionSuffix = string;
  this->UpdateDescriptionText();
}

//----------------------------------------------------------------------------
void DeviceSetSelectorWidget::ClearDescriptionSuffix()
{
  this->SetDescriptionSuffix("");
}