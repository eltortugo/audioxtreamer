// TortugASIO.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

/*
Steinberg Audio Stream I/O API
(c) 1996, Steinberg Soft- und Hardware GmbH
*/
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "TortugASIO.h"
//Threading
#include <process.h>

#include "timeapi.h"

#include "AudioXtreamer\ASIOSettingsFile.h"
#include "AudioXtreamer\ASIOSettings.h"

#include "AudioXtreamerDevice.h"
#include "fx2lp\cypressdevice.h"


using namespace ASIOSettings;




#define DEFAULT_SAMPLING_RATE 48000
#define NR_INS 32
#define NR_OUTS 32
#define NR_SAMPLES 64



//Time utility
static const double twoRaisedTo32 = 4294967296.0;
void getNanoSeconds(ASIOTimeStamp* ts)
{
  double nanoSeconds = (double)((unsigned long)timeGetTime()) * 1000000.;
  ts->hi = (unsigned long)(nanoSeconds / twoRaisedTo32);
  ts->lo = (unsigned long)(nanoSeconds - (ts->hi * twoRaisedTo32));
}
double AsioSamples2double(ASIOSamples* samples)
{
  double a = (double)(samples->lo);
  if (samples->hi)
    a += (double)(samples->hi) * twoRaisedTo32;
  return a;
}




#ifdef _WIN64
const TCHAR * dllName = _T("TortugASIOx64.dll");
const TCHAR * regKey = _T("TortugASIO x64 Driver");
const TCHAR * driverName = _T("TortugASIO USB FT2FPGA x64");
const wchar_t *className = L"TORTUGASIO64";
const TCHAR * ifaceName = _T("TortugASIOx64");
#else // !_WIN64
const TCHAR * dllName    = _T("TortugASIO.dll");
const TCHAR * regKey   = _T("TortugASIO Driver");
const TCHAR * driverName = _T("TortugASIO USB FT2FPGA");
const wchar_t *className = L"TORTUGASIO";
const TCHAR * ifaceName = _T("TortugASIO");
#endif



CFactoryTemplate g_Templates[1] = {
  { className, &IID_TORTUGASIO_XTREAMER, TortugASIO::CreateInstance }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

CUnknown* TortugASIO::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
  //DebugBreak();
  return (CUnknown*)new TortugASIO(pUnk, phr);
};

STDMETHODIMP TortugASIO::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
  //DebugBreak();
  if (riid == IID_TORTUGASIO_XTREAMER)
  {
    return GetInterface(this, ppv);
  }
  return CUnknown::NonDelegatingQueryInterface(riid, ppv);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//		Register ASIO Driver
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
extern LONG RegisterAsioDriver(const CLSID&, const TCHAR *, const TCHAR *, const TCHAR *, const TCHAR *);
extern LONG UnregisterAsioDriver(const CLSID&, const TCHAR *, const TCHAR *);

//
// Server registration, called on REGSVR32.EXE "the dllname.dll"
//
HRESULT _stdcall DllRegisterServer()
{
  LONG	rc;
  char	errstr[128];

  rc = RegisterAsioDriver(IID_TORTUGASIO_XTREAMER, dllName, regKey, driverName, _T("Apartment"));

  if (rc) {
    memset(errstr, 0, 128);
    sprintf(errstr, "Register Server failed ! (%d)", rc);
    MessageBox(0, (LPCTSTR)errstr, (LPCTSTR)"ASIO TortugASIO Driver", MB_OK);
    return -1;
  }

  return S_OK;
}

//
// Server unregistration
//
HRESULT _stdcall DllUnregisterServer()
{
  LONG	rc;
  char	errstr[128];

  rc = UnregisterAsioDriver(IID_TORTUGASIO_XTREAMER, dllName, regKey);

  if (rc) {
    memset(errstr, 0, 128);
    sprintf(errstr, "Unregister Server failed ! (%d)", rc);
    MessageBox(0, (LPCTSTR)errstr, (LPCTSTR)"ASIO TortugASIO Driver", MB_OK);
    return -1;
  }

  return S_OK;
}

//---------------------------------------------------------------------------------------------

void TortugASIO::Switch(uint32_t rxStride, uint8_t *rxBuff, uint32_t txStride, uint8_t *txBuff)
{

    unsigned char L;
    unsigned char M;
    unsigned char H;


    //the device accepts this format.
    // L1 M1 H1 L2 M2 H2... L24 M24 H24
    // N channels * MSB first, 24bit.

    if (WaitForSingleObject(hBuffMutex, 0) == WAIT_OBJECT_0)
    {
      if (bufferActive)
      {
        for (int s = 0; s < mNumSamples; s++)
        {
          for (int c = 0; c < mNumInputs; c++)
          {
            L = *(rxBuff + s*rxStride + c * 3 + 0);
            M = *(rxBuff + s*rxStride + c * 3 + 1);
            H = *(rxBuff + s*rxStride + c * 3 + 2);

            *(InputBuffers[c] + buffIdx*(mNumSamples * 3) + s * 3 + 0) = L;
            *(InputBuffers[c] + buffIdx*(mNumSamples * 3) + s * 3 + 1) = M;
            *(InputBuffers[c] + buffIdx*(mNumSamples * 3) + s * 3 + 2) = H;
          }
        }
      }
      ReleaseMutex(hBuffMutex);
    }

    if (callbacks)
    {
      getNanoSeconds(&theSystemTime);			// latch system time
      samplePosition += blockFrames;

      callbacks->bufferSwitch(buffIdx, ASIOTrue);
      //client reads the input data and fills the ouput data, no waiting allowed thus ASIOTrue
    }

    if (WaitForSingleObject(hBuffMutex, 0) == WAIT_OBJECT_0)
    {
      if (bufferActive)
      {
        for (int s = 0; s < mNumSamples; s++)
        {
          for (int c = 0; c < mNumOutputs; c++)
          {
            L = *(OutputBuffers[c] + buffIdx*(mNumSamples * 3) + s * 3 + 0);
            M = *(OutputBuffers[c] + buffIdx*(mNumSamples * 3) + s * 3 + 1);
            H = *(OutputBuffers[c] + buffIdx*(mNumSamples * 3) + s * 3 + 2);

            *(txBuff + s*txStride + c * 3 + 0) = L;
            *(txBuff + s*txStride + c * 3 + 1) = M;
            *(txBuff + s*txStride + c * 3 + 2) = H;
          }
        }
      }
      ReleaseMutex(hBuffMutex);
    }
    buffIdx ^= 1;
}

//------------------------------------------------------------------------------------------

void TortugASIO::AllocBuffers(uint32_t rxSize, uint8_t*&rxBuff, uint32_t txSize, uint8_t*&txBuff)
{
  rxBuff = (uint8_t*)malloc(rxSize);
  txBuff = (uint8_t*)malloc(txSize);
}

//------------------------------------------------------------------------------------------

void TortugASIO::FreeBuffers(uint8_t*&rxBuff, uint8_t*&txBuff)
{
  free(rxBuff);
  rxBuff = nullptr;
  free(txBuff);
  txBuff = nullptr;
}

//------------------------------------------------------------------------------------------

void TortugASIO::DeviceStopped(bool error)
{//called from the device's thread context
  if (error && callbacks && callbacks->asioMessage)
    callbacks->asioMessage(kAsioResetRequest, 0, 0, 0);
}

//------------------------------------------------------------------------------------------

ASIOSettings::Settings gSettings =
{
  { 15, 15, 15,_T("NrIns"),     _T("; zero index based number of pcm LR lines")},
  { 15, 15, 15,_T("NrOuts"),    _T("; zero index based number of pcm LR lines")},
  { 64, 64, 256,_T("NrSamples"), _T("; Whats necesary to run smooth and the least 512b usb packets")},
  { 64, 64, 256,_T("FifoSize"),  _T("; Size of the hardware Out FIFO")}
};

//------------------------------------------------------------------------------------------
TortugASIO::TortugASIO(LPUNKNOWN pUnk, HRESULT *phr)
: CUnknown(ifaceName, pUnk, phr)
, mNumSamples(gSettings[NrSamples].val)
{
  LOG0("TortugASIO::TortugASIO");

  // typically blockFrames * 2; try to get 1 by offering direct buffer
  // access, and using asioPostOutput for lower latency
  samplePosition = 0;
  sampleRate = DEFAULT_SAMPLING_RATE;
  milliSeconds = (long)((double)(mNumSamples * 1000) / sampleRate);
  active = false;
  started = false;
  bufferActive = false;

  OutputBuffers = nullptr;
  outMap = nullptr;

  InputBuffers = nullptr;
  inMap = nullptr;

  callbacks = 0;
  activeInputs = activeOutputs = 0;
  buffIdx = 0;

  mDevice = nullptr;
  hBuffMutex = INVALID_HANDLE_VALUE;


  mIniFile = new ASIOSettingsFile(gSettings);
  if (mIniFile) {
    mIniFile->Load();
    mNumInputs = (gSettings[NrIns].val+1)*2;
    mNumOutputs = (gSettings[NrOuts].val+1)*2;
  }

  blockFrames = mNumSamples;
}

//------------------------------------------------------------------------------------------
TortugASIO::~TortugASIO()
{
  LOG0("TortugASIO::~TortugASIO");

  if(started)
    stop();
  if (mDevice)
  {
    mDevice->Close();
    delete mDevice;
    mDevice = NULL;
  }

  disposeBuffers();
  if (hBuffMutex) {
    CloseHandle(hBuffMutex);
    hBuffMutex = INVALID_HANDLE_VALUE;
  }

  delete mIniFile;
  mIniFile = nullptr;
}

//------------------------------------------------------------------------------------------
void TortugASIO::getDriverName(char *name)
{
#ifdef _WIN64 
  const char * driverName = "TortugASIO USB FT2FPGA x64";
#else
  const char * driverName = "TortugASIO USB FT2FPGA";
#endif
  strcpy(name, driverName);
}

//------------------------------------------------------------------------------------------
long TortugASIO::getDriverVersion()
{
  return 0x00000001L;
}

//------------------------------------------------------------------------------------------
void TortugASIO::getErrorMessage(char *string)
{
  strcpy(string, errorMessage);
}

//------------------------------------------------------------------------------------------
ASIOBool TortugASIO::init(void* sysRef)
{
  LOG0("TortugASIO::init");
  bufferActive = false;

  if (active)
    return true;
  strcpy(errorMessage, "ASIO Driver open Failure!");
#if 0
  mDevice = new CypressDevice( *this, gSettings);
#else
  mDevice = new AudioXtreamerDevice(*this, gSettings);
#endif

  hBuffMutex = CreateMutex(NULL, FALSE, NULL);

  if (mDevice != nullptr && mDevice->Open()) {
    active = true;
    return true;
  }

  if(mDevice)
    delete mDevice;

  mDevice = nullptr;

  return false;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::start()
{
  LOG0("TortugASIO::start");
  if (callbacks && mDevice)
  {
    started = false;
    samplePosition = 0;
    theSystemTime.lo = theSystemTime.hi = 0;
    buffIdx = 0;

    if (mDevice->Start())
    {
      started = true;
      return ASE_OK;
    }
  }
  return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::stop()
{
  LOG0("TortugASIO::stop");
  if (mDevice && mDevice->Stop(false)) {
    started = false;
    return ASE_OK;
  }
  return ASE_HWMalfunction;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::getChannels(long *numInputChannels, long *numOutputChannels)
{
  LOG0("TortugASIO::getChannels");
  *numInputChannels = mNumInputs;
  //foobar?
  *numOutputChannels = mNumOutputs;
  //*numOutputChannels = 16;
  return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::getLatencies(long *_inputLatency, long *_outputLatency)
{
  *_inputLatency = blockFrames;		// typically;
  *_outputLatency = blockFrames + gSettings[FifoDepth].val;
  LOGN("TortugASIO::getLatencies %ld:%ld", *_inputLatency, *_outputLatency);

  return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::getBufferSize(long *minSize, long *maxSize,
  long *preferredSize, long *granularity)
{
  LOG0("TortugASIO::getBufferSize");
  *minSize = *maxSize = *preferredSize = blockFrames;//TODO: various sizes in case the ftdi or fpga dont collaborate
  *granularity = 0;
  return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::canSampleRate(ASIOSampleRate sampleRate)
{
  LOG0("TortugASIO::canSampleRate");
  if (sampleRate == DEFAULT_SAMPLING_RATE)		// TODO: allow these rates only, parse the response from the fpga looking for the sampling rate
    return ASE_OK;
  return ASE_NoClock;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::getSampleRate(ASIOSampleRate *sampleRate)
{
  LOG0("TortugASIO::getSampleRate");
  *sampleRate = this->sampleRate;
  return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::setSampleRate(ASIOSampleRate sampleRate)
{
  LOG0("TortugASIO::setSampleRate");
  if (sampleRate != DEFAULT_SAMPLING_RATE )
    return ASE_NoClock;

  return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::getClockSources(ASIOClockSource *clocks, long *numSources)
{
  LOG0("TortugASIO::getClockSources");
  // return as internal
  clocks->index = 0;
  clocks->associatedChannel = -1;
  clocks->associatedGroup = -1;
  clocks->isCurrentSource = ASIOTrue;
  strcpy(clocks->name, "Internal");
  *numSources = 1;
  return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError TortugASIO::setClockSource(long index)
{
  if (!index)
  {
    return ASE_OK;
  }
  return ASE_NotPresent;
}


//------------------------------------------------------------------------------------------
ASIOError TortugASIO::getChannelInfo(ASIOChannelInfo *info)
{
  if (info->channel < 0 || (info->isInput ? info->channel >= mNumInputs : info->channel >= mNumOutputs))
    return ASE_InvalidParameter;

  info->type = ASIOSTInt24LSB;
  info->channelGroup = 0;
  info->isActive = ASIOFalse;
  long i;
  if (info->isInput)
  {
    for (i = 0; i < activeInputs; i++)
    {
      if (inMap != nullptr && inMap[i] == info->channel)
      {
        info->isActive = ASIOTrue;
        break;
      }
    }
    sprintf(info->name, "Y01XUSB In %d", info->channel + 1); //channel = from 0
  }
  else
  {
    for (i = 0; i < activeOutputs; i++)
    {
      if (outMap != nullptr && outMap[i] == info->channel)
      {
        info->isActive = ASIOTrue;
        break;
      }
    }
    sprintf(info->name, "Y01XUSB Out %d", info->channel + 1); //channel = from 0
  }
  return ASE_OK;
}

ASIOError TortugASIO::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp)
{
  tStamp->lo = theSystemTime.lo;
  tStamp->hi = theSystemTime.hi;
  if (samplePosition >= twoRaisedTo32)
  {
    sPos->hi = (unsigned long)(samplePosition * (1.0 / twoRaisedTo32));
    sPos->lo = (unsigned long)(samplePosition - (sPos->hi * twoRaisedTo32));
  }
  else
  {
    sPos->hi = 0;
    sPos->lo = (unsigned long)samplePosition;
  }
  return ASE_OK;
}
//------------------------------------------------------------------------------------------
ASIOError TortugASIO::createBuffers(ASIOBufferInfo *bufferInfos, long numChannels,
  long bufferSize, ASIOCallbacks *callbacks)
{
  LOG0("TortugASIO::createBuffers");
  ASIOBufferInfo *info = bufferInfos;
  long i;

  activeInputs = 0;
  activeOutputs = 0;
  blockFrames = bufferSize;

  OutputBuffers = new uint8_t*[mNumOutputs];
  outMap = new long[mNumOutputs];
  for (i = 0; i < mNumOutputs; i++)
  {
    OutputBuffers[i] = new uint8_t[blockFrames * 3 * 2];
    memset(OutputBuffers[i], 0, blockFrames * 3 * 2);
    outMap[i] = -1;
  }

  InputBuffers = new uint8_t*[mNumInputs];
  inMap = new long[mNumInputs];
  for (i = 0; i < mNumInputs; i++)
  {
    InputBuffers[i] = new uint8_t[blockFrames * 3 * 2];
    memset(InputBuffers[i], 0, blockFrames * 3 * 2);
    inMap[i] = -1;
  }

  for (i = 0; i < numChannels; i++, info++)
  {
    if (info->isInput )
    {
      if (info->channelNum >= 0 && info->channelNum < mNumInputs) {
        info->buffers[0] = InputBuffers[info->channelNum];
        info->buffers[1] = InputBuffers[info->channelNum] + blockFrames * 3;
        inMap[activeInputs] = info->channelNum;
        activeInputs++;
      }
    }else
    {
      if (info->channelNum >= 0 && info->channelNum < mNumOutputs) {
        info->buffers[0] = OutputBuffers[info->channelNum];
        info->buffers[1] = OutputBuffers[info->channelNum] + blockFrames * 3;
        outMap[activeOutputs] = info->channelNum;
        activeOutputs++;
      }
    }
  }

  this->callbacks = callbacks;
  bufferActive = true;
  if (callbacks->asioMessage != NULL) {
    if (callbacks->asioMessage(kAsioSupportsTimeInfo, 0, 0, 0))
    {
      timeInfoMode = true;
      asioTime.timeInfo.speed = 1.;
      asioTime.timeInfo.systemTime.hi = asioTime.timeInfo.systemTime.lo = 0;
      asioTime.timeInfo.samplePosition.hi = asioTime.timeInfo.samplePosition.lo = 0;
      asioTime.timeInfo.sampleRate = sampleRate;
      asioTime.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;

      asioTime.timeCode.speed = 1.;
      asioTime.timeCode.timeCodeSamples.lo = asioTime.timeCode.timeCodeSamples.hi = 0;
      asioTime.timeCode.flags = kTcValid | kTcRunning;
    }
  }
  else {
    timeInfoMode = false;
  }

  return ASE_OK;
}

//---------------------------------------------------------------------------------------------
ASIOError TortugASIO::disposeBuffers()
{
  LOG0("TortugASIO::disposeBuffers");
  long i;

  if (bufferActive)
  {
    if (WaitForSingleObject(hBuffMutex, INFINITE) == WAIT_OBJECT_0) {
      for (i = 0; i < mNumOutputs; i++)
      {
        if (OutputBuffers[i])
          delete(OutputBuffers[i]);
      }
      delete OutputBuffers;
      OutputBuffers = nullptr;
      delete outMap;
      outMap = nullptr;

      for (i = 0; i < mNumInputs; i++)
      {
        if (InputBuffers[i])
          delete(InputBuffers[i]);
      }
      delete InputBuffers;
      InputBuffers = nullptr;
      delete inMap;
      inMap = nullptr;

      bufferActive = false;
      callbacks = 0;
      activeOutputs = 0;
      activeInputs = 0;
      ReleaseMutex(hBuffMutex);
    }
  }
 
  return ASE_OK;
}

//---------------------------------------------------------------------------------------------
ASIOError TortugASIO::controlPanel()
{
  //send message to AudioXtreamer and wait for the completion of the dialog
  if ( mDevice != nullptr && mDevice->ConfigureDevice() && mIniFile->Load())
  {
    mNumInputs = (gSettings[NrIns].val+1)*2;
    mNumOutputs = (gSettings[NrOuts].val+1)*2;
    blockFrames = mNumSamples;
    if(mIniFile) 
      mIniFile->Save();

    if (callbacks && callbacks->asioMessage)
      callbacks->asioMessage(kAsioResetRequest, 0, 0, 0);
  }

  return ASE_OK;
}

//---------------------------------------------------------------------------------------------
ASIOError TortugASIO::future(long selector, void* opt)	// !!! check properties 
{
  return ASE_NotPresent;
}

//--------------------------------------------------------------------------------------------------------
// private methods
//--------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
ASIOError TortugASIO::outputReady()
{
  return ASE_OK;
}





