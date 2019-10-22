#pragma once

namespace ASIOSettings
{
  enum Setting{
    NrIns = 0,
    NrOuts = 1,
    NrSamples = 2,
    FifoDepth = 3,
    MaxSetting = 4
  };

  typedef struct _Settings {
    int val;
    const int def;
    const int max;
    const LPCTSTR key;
    const LPCTSTR desc;
  } Settings[MaxSetting];

#pragma pack(push,4)

  typedef struct _StreamInfo {
    uint32_t RxStride;
    uint32_t RxOffset;
    uint32_t TxStride;
    uint32_t TxOffset;
    uint32_t Flags;
  } StreamInfo;

#pragma pack(pop)

  static const uint8_t ChanEntires = 16;
};

#define WM_XTREAMER WM_APP + 100

//1MB shared memory for asio buffers
//should suffice for a 16x deep queue 64ch @ 256samples
#define SH_MEM_BLK_SIZE_SHIFT 20 


extern const CLSID IID_TORTUGASIO_XTREAMER;
extern LPCTSTR const szNameShMem;
extern LPCTSTR const szNameAsioEvent;
extern LPCTSTR const szNameXtreamerEvent;
extern LPCTSTR const szNameClass;
extern LPCTSTR const szNameApp;

extern ASIOSettings::Settings theSettings;

