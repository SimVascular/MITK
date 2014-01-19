/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include "mitkUSDevice.h"
#include "mitkImageReadAccessor.h"

// US Control Interfaces
#include "mitkUSControlInterfaceProbes.h"
#include "mitkUSControlInterfaceBMode.h"
#include "mitkUSControlInterfaceDoppler.h"

// Microservices
#include <usGetModuleContext.h>
#include <usModule.h>
#include <usServiceProperties.h>
#include <usModuleContext.h>

const std::string yes = "true";
const std::string no = "false";

const std::string mitk::USDevice::US_INTERFACE_NAME = "org.mitk.services.UltrasoundDevice";
const std::string mitk::USDevice::US_PROPKEY_LABEL = US_INTERFACE_NAME + ".label";
const std::string mitk::USDevice::US_PROPKEY_ISCONNECTED = US_INTERFACE_NAME + ".isConnected";
const std::string mitk::USDevice::US_PROPKEY_ISACTIVE = US_INTERFACE_NAME + ".isActive";
const std::string mitk::USDevice::US_PROPKEY_CLASS = US_INTERFACE_NAME + ".class";

const std::string mitk::USDevice::US_PROPKEY_PROBES_SELECTED = US_INTERFACE_NAME + ".probes.selected";

const std::string mitk::USDevice::US_PROPKEY_BMODE_FREQUENCY = US_INTERFACE_NAME + ".bmode.frequency";
const std::string mitk::USDevice::US_PROPKEY_BMODE_POWER = US_INTERFACE_NAME + ".bmode.power";
const std::string mitk::USDevice::US_PROPKEY_BMODE_DEPTH = US_INTERFACE_NAME + ".bmode.depth";
const std::string mitk::USDevice::US_PROPKEY_BMODE_GAIN = US_INTERFACE_NAME + ".bmode.gain";
const std::string mitk::USDevice::US_PROPKEY_BMODE_REJECTION = US_INTERFACE_NAME + ".bmode.rejection";

mitk::USDevice::USImageCropArea mitk::USDevice::GetCropArea()
{
  MITK_INFO << "Return Crop Area L:" << m_CropArea.cropLeft
    << " R:" << m_CropArea.cropRight << " T:" << m_CropArea.cropTop
    << " B:" << m_CropArea.cropBottom;
  return m_CropArea;
}

mitk::USDevice::USDevice(std::string manufacturer, std::string model)
  : mitk::ImageSource(),
  m_IsFreezed(false),
  m_DeviceState(State_NoState),
  m_SpawnAcquireThread(true),
  m_MultiThreader(itk::MultiThreader::New()),
  m_ImageMutex(itk::FastMutexLock::New()),
  m_ThreadID(-1),
  m_UnregisteringStarted(false)
{
  // Initialize Members
  m_Metadata = mitk::USImageMetadata::New();
  m_Metadata->SetDeviceManufacturer(manufacturer);
  m_Metadata->SetDeviceModel(model);

  USImageCropArea empty;
  empty.cropBottom = 0;
  empty.cropTop = 0;
  empty.cropLeft = 0;
  empty.cropRight = 0;
  this->m_CropArea = empty;

  //set number of outputs
  this->SetNumberOfIndexedOutputs(1);

  //create a new output
  mitk::Image::Pointer newOutput = mitk::Image::New();
  this->SetNthOutput(0,newOutput);
}

mitk::USDevice::USDevice(mitk::USImageMetadata::Pointer metadata)
  : mitk::ImageSource(),
  m_IsFreezed(false),
  m_DeviceState(State_NoState),
  m_SpawnAcquireThread(true),
  m_MultiThreader(itk::MultiThreader::New()),
  m_ImageMutex(itk::FastMutexLock::New()),
  m_ThreadID(-1),
  m_UnregisteringStarted(false)
{
  m_Metadata = metadata;

  USImageCropArea empty;
  empty.cropBottom = 0;
  empty.cropTop = 0;
  empty.cropLeft = 0;
  empty.cropRight = 0;
  this->m_CropArea = empty;

  //set number of outputs
  this->SetNumberOfIndexedOutputs(1);

  //create a new output
  mitk::Image::Pointer newOutput = mitk::Image::New();
  this->SetNthOutput(0,newOutput);
}

mitk::USDevice::~USDevice()
{
  if (m_ThreadID >= 0)
  {
    m_MultiThreader->TerminateThread(m_ThreadID);
  }

  // make sure that the us device is not registered at the micro service
  // anymore after it is destructed
  this->UnregisterOnService();
}

mitk::USAbstractControlInterface::Pointer mitk::USDevice::GetControlInterfaceCustom()
{
  MITK_INFO << "Custom control interface does not exist for this object.";
  return 0;
}

mitk::USControlInterfaceBMode::Pointer mitk::USDevice::GetControlInterfaceBMode()
{
  MITK_INFO << "Control interface BMode does not exist for this object.";
  return 0;
}

mitk::USControlInterfaceProbes::Pointer mitk::USDevice::GetControlInterfaceProbes()
{
  MITK_INFO << "Control interface Probes does not exist for this object.";
  return 0;
}

mitk::USControlInterfaceDoppler::Pointer mitk::USDevice::GetControlInterfaceDoppler()
{
  MITK_INFO << "Control interface Doppler does not exist for this object.";
  return 0;
}

us::ServiceProperties mitk::USDevice::ConstructServiceProperties()
{
  us::ServiceProperties props;

  props[mitk::USDevice::US_PROPKEY_ISCONNECTED] = this->GetIsConnected() ? yes : no;
  props[mitk::USDevice::US_PROPKEY_ISACTIVE] = this->GetIsActive() ? yes : no;

  props[mitk::USDevice::US_PROPKEY_LABEL] = this->GetServicePropertyLabel();

  // get identifier of selected probe if there is one selected
  mitk::USControlInterfaceProbes::Pointer probesControls = this->GetControlInterfaceProbes();
  if (probesControls.IsNotNull() && probesControls->GetIsActive())
  {
    mitk::USProbe::Pointer probe = probesControls->GetSelectedProbe();
    if (probe.IsNotNull())
    {
      props[mitk::USDevice::US_PROPKEY_PROBES_SELECTED] = probe->GetName();
    }
  }

  //TODO Handle in subclasses?
  //props[ mitk::USImageMetadata::PROP_DEV_ISCALIBRATED ] = m_Calibration.IsNotNull() ? yes : no;

  props[ mitk::USDevice::US_PROPKEY_CLASS ] = GetDeviceClass();
  props[ mitk::USImageMetadata::PROP_DEV_MANUFACTURER ] = m_Metadata->GetDeviceManufacturer();
  props[ mitk::USImageMetadata::PROP_DEV_MODEL ] = m_Metadata->GetDeviceModel();
  props[ mitk::USImageMetadata::PROP_DEV_COMMENT ] = m_Metadata->GetDeviceComment();
  props[ mitk::USImageMetadata::PROP_PROBE_NAME ] = m_Metadata->GetProbeName();
  props[ mitk::USImageMetadata::PROP_PROBE_FREQUENCY ] = m_Metadata->GetProbeFrequency();
  props[ mitk::USImageMetadata::PROP_ZOOM ] = m_Metadata->GetZoom();

  m_ServiceProperties = props;

  return props;
}

void mitk::USDevice::UnregisterOnService()
{
  // unregister on micro service
  if ( m_ServiceRegistration && ! m_UnregisteringStarted)
  {
    // make sure that unregister is not started a second
    // time due to a callback during unregister for example
    m_UnregisteringStarted = true;
    m_ServiceRegistration.Unregister();
    m_ServiceRegistration = 0;
  }
}

bool mitk::USDevice::Initialize()
{
  if (! this->OnInitialization() ) { return false; }

  m_DeviceState = State_Initialized;

  // Get Context and Module
  us::ModuleContext* context = us::GetModuleContext();
  us::ServiceProperties props = this->ConstructServiceProperties();

  m_ServiceRegistration = context->RegisterService(this, props);

  return true;
}

bool mitk::USDevice::Connect()
{
  if ( this->GetIsConnected() )
  {
    MITK_INFO("mitkUSDevice") << "Tried to connect an ultrasound device that was already connected. Ignoring call...";
    return true;
  }

  if ( ! this->GetIsInitialized() )
  {
    MITK_ERROR("mitkUSDevice") << "Cannot connect device if it is not in initialized state.";
    return false;
  }

  // Prepare connection, fail if this fails.
  if ( ! this->OnConnection() ) { return false; }

  // Update state
  m_DeviceState = State_Connected;

  this->UpdateServiceProperty(mitk::USDevice::US_PROPKEY_ISCONNECTED, true);
  return true;
}

void mitk::USDevice::ConnectAsynchron()
{
  this->m_MultiThreader->SpawnThread(this->ConnectThread, this);
}

bool mitk::USDevice::Disconnect()
{
  if ( ! GetIsConnected())
  {
    MITK_WARN << "Tried to disconnect an ultrasound device that was not connected. Ignoring call...";
    return false;
  }
  // Prepare connection, fail if this fails.
  if (! this->OnDisconnection()) return false;

  // Update state
  m_DeviceState = State_Initialized;

  this->UpdateServiceProperty(mitk::USDevice::US_PROPKEY_ISCONNECTED, false);

  return true;
}

bool mitk::USDevice::Activate()
{
  if (! this->GetIsConnected())
  {
    MITK_INFO("mitkUSDevice") << "Cannot activate device if it is not in connected state.";
    return true;
  }

  if ( OnActivation() )
  {
    m_DeviceState = State_Activated;

    m_FreezeBarrier = itk::ConditionVariable::New();

    // spawn thread for aquire images if us device is active
    if (m_SpawnAcquireThread)
    {
      this->m_ThreadID = this->m_MultiThreader->SpawnThread(this->Acquire, this);
    }

    this->UpdateServiceProperty(mitk::USDevice::US_PROPKEY_ISACTIVE, true);
    this->UpdateServiceProperty(mitk::USDevice::US_PROPKEY_LABEL, this->GetServicePropertyLabel());
  }

  return m_DeviceState == State_Activated;
}

void mitk::USDevice::Deactivate()
{
  if ( ! this->GetIsActive() )
  {
    MITK_WARN("mitkUSDevice")
      << "Cannot deactivate a device which is not activae.";
    return;
  }

  if ( ! OnDeactivation() ) { return; }

  m_DeviceState = State_Connected;

  this->UpdateServiceProperty(mitk::USDevice::US_PROPKEY_ISACTIVE, false);
  this->UpdateServiceProperty(mitk::USDevice::US_PROPKEY_LABEL, this->GetServicePropertyLabel());
}

void mitk::USDevice::SetIsFreezed(bool freeze)
{
  if ( ! this->GetIsActive() )
  {
    MITK_WARN("mitkUSDevice")
      << "Cannot freeze or unfreeze if device is not active.";
    return;
  }

  this->OnFreeze(freeze);

  if ( freeze )
  {
    m_IsFreezed = true;
  }
  else
  {
    m_IsFreezed = false;

    // wake up the image acquisition thread
    m_FreezeBarrier->Signal();
  }
}

bool mitk::USDevice::GetIsFreezed()
{
  if ( ! this->GetIsActive() )
  {
    MITK_WARN("mitkUSDevice")("mitkUSTelemedDevice")
      << "Cannot get freeze state if the hardware interface is not ready. Returning false...";
    return false;
  }

  return m_IsFreezed;
}

void mitk::USDevice::PushFilter(AbstractOpenCVImageFilter::Pointer filter)
{
  mitk::USImageSource::Pointer imageSource = this->GetUSImageSource();
  if ( imageSource.IsNull() )
  {
    MITK_ERROR << "ImageSource must not be null when pushing a filter.";
    mitkThrow() << "ImageSource must not be null when pushing a filter.";
  }

  imageSource->PushFilter(filter);
}

void mitk::USDevice::PushFilterIfNotPushedBefore(AbstractOpenCVImageFilter::Pointer filter)
{
  mitk::USImageSource::Pointer imageSource = this->GetUSImageSource();
  if ( imageSource.IsNull() )
  {
    MITK_ERROR << "ImageSource must not be null when pushing a filter.";
    mitkThrow() << "ImageSource must not be null when pushing a filter.";
  }

  if ( ! imageSource->GetIsFilterInThePipeline(filter) )
  {
    imageSource->PushFilter(filter);
  }
}

bool mitk::USDevice::RemoveFilter(AbstractOpenCVImageFilter::Pointer filter)
{
  mitk::USImageSource::Pointer imageSource = this->GetUSImageSource();
  if ( imageSource.IsNull() )
  {
    MITK_ERROR << "ImageSource must not be null when pushing a filter.";
    mitkThrow() << "ImageSource must not be null when removing a filter.";
  }

  return imageSource->RemoveFilter(filter);
}

void mitk::USDevice::UpdateServiceProperty(std::string key, std::string value)
{
  m_ServiceProperties[ key ] = value;
  m_ServiceRegistration.SetProperties(m_ServiceProperties);
}

void mitk::USDevice::UpdateServiceProperty(std::string key, double value)
{
  std::stringstream stream;
  stream << value;
  this->UpdateServiceProperty(key, stream.str());
}

void mitk::USDevice::UpdateServiceProperty(std::string key, bool value)
{
  this->UpdateServiceProperty(key, value ? std::string("true") : std::string("false"));
}

/**
mitk::Image* mitk::USDevice::GetOutput()
{
if (this->GetNumberOfOutputs() < 1)
return NULL;

return static_cast<USImage*>(this->ProcessObject::GetPrimaryOutput());
}

mitk::Image* mitk::USDevice::GetOutput(unsigned int idx)
{
if (this->GetNumberOfOutputs() < 1)
return NULL;
return static_cast<USImage*>(this->ProcessObject::GetOutput(idx));
}

void mitk::USDevice::GraftOutput(itk::DataObject *graft)
{
this->GraftNthOutput(0, graft);
}

void mitk::USDevice::GraftNthOutput(unsigned int idx, itk::DataObject *graft)
{
if ( idx >= this->GetNumberOfOutputs() )
{
itkExceptionMacro(<<"Requested to graft output " << idx <<
" but this filter only has " << this->GetNumberOfOutputs() << " Outputs.");
}

if ( !graft )
{
itkExceptionMacro(<<"Requested to graft output with a NULL pointer object" );
}

itk::DataObject* output = this->GetOutput(idx);
if ( !output )
{
itkExceptionMacro(<<"Requested to graft output that is a NULL pointer" );
}
// Call Graft on USImage to copy member data
output->Graft( graft );
}

*/

void mitk::USDevice::GrabImage()
{
  mitk::Image::Pointer image = this->GetUSImageSource()->GetNextImage();
  m_ImageMutex->Lock();
  this->SetImage(image);
  m_ImageMutex->Unlock();
}

//########### GETTER & SETTER ##################//

bool mitk::USDevice::GetIsInitialized()
{
  return m_DeviceState == State_Initialized;
}

bool mitk::USDevice::GetIsActive()
{
  return m_DeviceState == State_Activated;
}

bool mitk::USDevice::GetIsConnected()
{
  return m_DeviceState == State_Connected;
}

std::string mitk::USDevice::GetDeviceManufacturer(){
  return this->m_Metadata->GetDeviceManufacturer();
}

std::string mitk::USDevice::GetDeviceModel(){
  return this->m_Metadata->GetDeviceModel();
}

std::string mitk::USDevice::GetDeviceComment(){
  return this->m_Metadata->GetDeviceComment();
}

void mitk::USDevice::GenerateData()
{
  m_ImageMutex->Lock();

  if ( m_Image.IsNull() || ! m_Image->IsInitialized() ) { m_ImageMutex->Unlock(); return; }

  mitk::Image::Pointer output = this->GetOutput();

  if ( ! output->IsInitialized()
    || output->GetDimension(0) != m_Image->GetDimension(0) || output->GetDimension(1) != m_Image->GetDimension(1) )
  {
    output->Initialize(m_Image->GetPixelType(), m_Image->GetDimension(), m_Image->GetDimensions());
  }

  mitk::ImageReadAccessor inputReadAccessor(m_Image, m_Image->GetSliceData(0,0,0));
  output->SetSlice(inputReadAccessor.GetData());

  m_ImageMutex->Unlock();
};

std::string mitk::USDevice::GetServicePropertyLabel()
{
  std::string isActive;
  if (this->GetIsActive()) { isActive = " (Active)"; }
  else { isActive = " (Inactive)"; }
  // e.g.: Zonare MyLab5 (Active)
  return m_Metadata->GetDeviceManufacturer() + " " + m_Metadata->GetDeviceModel() + isActive;
}

ITK_THREAD_RETURN_TYPE mitk::USDevice::Acquire(void* pInfoStruct)
{
  /* extract this pointer from Thread Info structure */
  struct itk::MultiThreader::ThreadInfoStruct * pInfo = (struct itk::MultiThreader::ThreadInfoStruct*)pInfoStruct;
  mitk::USDevice* device = (mitk::USDevice*) pInfo->UserData;
  while (device->GetIsActive())
  {
    // lock this thread when ultrasound device is freezed
    if ( device->m_IsFreezed )
    {
      itk::SimpleMutexLock* mutex = &(device->m_FreezeMutex);
      mutex->Lock();

      if (device->m_FreezeBarrier.IsNotNull())
      {
        device->m_FreezeBarrier->Wait(mutex);
      }
    }

    device->GrabImage();
  }
  return ITK_THREAD_RETURN_VALUE;
}

ITK_THREAD_RETURN_TYPE mitk::USDevice::ConnectThread(void* pInfoStruct)
{
  /* extract this pointer from Thread Info structure */
  struct itk::MultiThreader::ThreadInfoStruct * pInfo = (struct itk::MultiThreader::ThreadInfoStruct*)pInfoStruct;
  mitk::USDevice* device = (mitk::USDevice*) pInfo->UserData;

  device->Connect();

  return ITK_THREAD_RETURN_VALUE;
}
