#pragma once
#include "combase.h"
#include "asiosys.h"
#include "iasiodrv.h"
#include <stdint.h>

#include "UsbDev\UsbDev.h"

class ASIOSettingsFile;
class TortugASIO : public IASIO, public CUnknown, public UsbDeviceClient
{

public:
  TortugASIO(LPUNKNOWN pUnk, HRESULT *phr);
  ~TortugASIO();
  DECLARE_IUNKNOWN
  static CUnknown *CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid, void **ppvObject);
  ASIOBool init(void* sysRef);
  void getDriverName(char *name);	// max 32 bytes incl. terminating zero
  long getDriverVersion();
  void getErrorMessage(char *string);	// max 128 bytes incl.

  ASIOError start();
  ASIOError stop();

  ASIOError getChannels(long *numInputChannels, long *numOutputChannels);
  ASIOError getLatencies(long *inputLatency, long *outputLatency);
  ASIOError getBufferSize(long *minSize, long *maxSize,
    long *preferredSize, long *granularity);

  ASIOError canSampleRate(ASIOSampleRate sampleRate);
  ASIOError getSampleRate(ASIOSampleRate *sampleRate);
  ASIOError setSampleRate(ASIOSampleRate sampleRate);

  ASIOError getClockSources(ASIOClockSource *clocks, long *numSources);
  ASIOError setClockSource(long index);
  ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp);

  ASIOError getChannelInfo(ASIOChannelInfo *info);

  ASIOError createBuffers(ASIOBufferInfo *bufferInfos, long numChannels,
    long bufferSize, ASIOCallbacks *callbacks);
  ASIOError disposeBuffers();

  ASIOError controlPanel();
  ASIOError future(long selector, void *opt);
  ASIOError outputReady();

  bool Switch(uint32_t timeout, uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) override;
  void AllocBuffers(uint32_t rxSize, uint8_t *&rxBuff, uint32_t txSize, uint8_t *&txBuff) override;
  void FreeBuffers(uint8_t *&rxBuff, uint8_t *&txBuff) override;
  void DeviceStopped(bool error) override;
  void SampleRateChanged() override;

  double samplePosition;

  ASIOTime asioTime;
  ASIOTimeStamp theSystemTime;

  ASIOCallbacks *callbacks;

  //This is internal channel data buffer. number of Outputs.
  uint8_t	mNumInputs;
  uint8_t	mNumOutputs;
  int &mNumSamples;
  uint8_t **OutputBuffers;
  uint8_t **InputBuffers;

  long *outMap;
  long *inMap;


  long blockFrames;
  long inputLatency;
  long outputLatency;
  long activeInputs;
  long activeOutputs;
  long buffIdx;

  volatile bool active, started;
  char errorMessage[128];
  bool timeInfoMode, tcRead;
  volatile bool bufferActive;

  UsbDevice * mDevice;

  ASIOSettingsFile * mIniFile;
  HANDLE hBuffMutex;

};