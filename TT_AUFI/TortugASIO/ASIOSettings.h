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

  static const uint8_t ChanEntires = 16;
};

