#include "stdafx.h"

#include <process.h>
#include <stdio.h>


#include "CypressDevice.h"

#include "UsbBackend.h"

#include "ZTEXDev\ztexdev.h"
#include "resource.h"


#include <tchar.h>


#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Rpcrt4.lib")

#include "avrt.h"
#pragma comment(lib, "avrt.lib")


int gcd(int a, int b)
{
  int r; // remainder
  while (b > 0) {
    r = a % b;
    a = b;
    b = r;
  }
  return a;
}

#define LCM(a,b) (((a)*(b))/gcd(a,b))

using namespace ASIOSettings;

uint8_t setyb[256];

CypressDevice::CypressDevice(UsbDeviceClient & client, ASIOSettings::Settings &params )
  : UsbDevice(client,params)
  , asioInPtr(nullptr)
  , asioOutPtr(nullptr)
  , mDevStatus({ 0 })
  , mFileHandle(NULL)
  , mASIOHandle(NULL)
  , mTxRequests(nullptr)
  , mRxRequests(nullptr)
{
  LOG0("CypressDevice::CypressDevice");

  mDefOutEP = 0;
  mDefInEP = 0;
  mDevHandle = INVALID_HANDLE_VALUE;
  hth_Worker = INVALID_HANDLE_VALUE;
  mExitHandle = INVALID_HANDLE_VALUE;


  hSem = CreateSemaphore(
    NULL,           // default security attributes
    1,  // initial count
    1,  // maximum count
    NULL);

  for (int b = 0; b < 256; ++b)
  {
    setyb [b] = ((b & 128) >> 7) |
      ((b & 64) >> 5) |
      ((b & 32) >> 3) |
      ((b & 16) >> 1) |
      ((b & 8) << 1) |
      ((b & 4) << 3) |
      ((b & 2) << 5) |
      ((b & 1) << 7);
  }

  HRSRC hrc = FindResource(NULL, MAKEINTRESOURCE(IDR_FPGA_BIN), _T("RC_DATA"));
  HGLOBAL hg = LoadResource(NULL, hrc);
  uint8_t* bits = (uint8_t*)LockResource(hg);
  mBitstream = nullptr;

  if (bits)
  {
    mResourceSize = SizeofResource(NULL, hrc);
    
    uint8_t * bitstream = (uint8_t *)malloc(mResourceSize +512);
    ZeroMemory(bitstream, 512);

    uint8_t* buf = bitstream + 512;
    for (uint32_t c = 0; c < mResourceSize; c++, buf++) {
      register uint8_t b = bits[c];
      *buf = setyb[b];
    }

    mBitstream = bitstream;
    mResourceSize += 512;
  }
}

CypressDevice::~CypressDevice()
{
  LOG0("CypressDevice::~CypressDevice");

  if (mBitstream) {
    free(mBitstream);
    mBitstream = nullptr;
  }

  if (mExitHandle != INVALID_HANDLE_VALUE)
    DebugBreak();


  CloseHandle(hSem);
}

bool CypressDevice::Start()
{
  LOG0("CypressDevice::Start");
  if (mDevHandle == INVALID_HANDLE_VALUE)
    return false;
  //let's try to identify the fpga
  int64_t get_result = ztex_default_lsi_get1(mDevHandle, 0);
  static const char* trtg = "TRTG";
  if (get_result != *(uint32_t*)trtg )
    return false;


  if (hth_Worker != INVALID_HANDLE_VALUE) {
    DWORD dwExCode;
    GetExitCodeThread(hth_Worker, &dwExCode);
    if (dwExCode == STILL_ACTIVE)
      return true;
  }

  hth_Worker = (HANDLE)_beginthread(StaticWorkerThread, 0, this);
  if (hth_Worker != INVALID_HANDLE_VALUE) {
    ::SetThreadPriority(hth_Worker, THREAD_PRIORITY_TIME_CRITICAL);
    return true;
  }
  return false;
}

bool CypressDevice::Stop(bool wait)
{
  LOG0("CypressDevice::Stop");

  
  if (mExitHandle != INVALID_HANDLE_VALUE) {
    BOOL result = SetEvent(mExitHandle);
    if (result == 0) {
      LOG0("CypressDevice::Stop SetEvent Exit FAILED!");
      wait = false;
    }

    if (wait) {
      WaitForSingleObject(hth_Worker, INFINITE);
      hth_Worker = INVALID_HANDLE_VALUE;
    }

    return true;
  }
  else
    return false;
}

//---------------------------------------------------------------------------------------------

bool CypressDevice::Open()
{
  LOG0("CypressDevice::Open");
  HANDLE handle = INVALID_HANDLE_VALUE;
  ztex_device_info info;
  mDefOutEP = 0;
  mDefInEP = 0;
  memset(&info, 0, sizeof(ztex_device_info));

  if (!bknd_open(handle, mFileHandle))
    goto err;

  int status = ztex_get_device_info(handle, &info);
  if (status < 0) {
    fprintf(stderr, "Error: Unable to get device info\n");
    goto err;
  }

  mDefInEP = 0x82;//info.default_in_ep;
  mDefOutEP = 0x8;//info.default_out_ep;

  /*status = ztex_get_fpga_config(handle);
  if (status == -1)
    goto err;
  else if (status == 0)*/
  {

   if (mBitstream != nullptr) {
#define EP0_TRANSACTION_SIZE 2048

      // reset FPGA
      status = (BOOL)control_transfer(handle, 0x40, 0x31, 0, 0, NULL, 0, 1500);
      // transfer data
      status = 0;
      uint32_t last_idx = mResourceSize % EP0_TRANSACTION_SIZE;
      int bufs_idx = (mResourceSize / EP0_TRANSACTION_SIZE);

      for (int i = 0; (status >= 0) && (i < bufs_idx); i++)
        status =(BOOL)control_transfer(handle, 0x40, 0x32, 0, 0, mBitstream + (i * EP0_TRANSACTION_SIZE), EP0_TRANSACTION_SIZE, 1500);

      if (last_idx)
        status =(BOOL)control_transfer(handle, 0x40, 0x32, 0, 0, mBitstream + bufs_idx * EP0_TRANSACTION_SIZE, last_idx, 1500);

    } else {
     goto err;
    }

    fflush(stderr);
    // check config
    status = ztex_get_fpga_config(handle);
    if (status < 0) {
      fprintf(stderr, "Error: Unable to get FPGA configuration state\n");
      goto err;
    }
    else if (status == 0) {
      fprintf(stderr, "Error: FPGA not configured\n");
      goto err;
    }
  }

  ztex_default_reset(handle, 0);

  status = 0;
  goto noerr;

err:
  status = 1;
  if (handle != INVALID_HANDLE_VALUE)
    bknd_close(handle, mFileHandle);

noerr:

  mDevHandle = status == 0 ? handle : INVALID_HANDLE_VALUE;
  if (status == 0)
    return true;
  else
    return false;
}

//---------------------------------------------------------------------------------------------
bool CypressDevice::Close()
{
  LOG0("CypressDevice::Close");
  //Check Thread Handle
  DWORD dwExCode;

  if (hth_Worker != INVALID_HANDLE_VALUE) {
    GetExitCodeThread(hth_Worker, &dwExCode);
    if (dwExCode == STILL_ACTIVE)
      Stop(true);
  }

  if (mDevHandle != INVALID_HANDLE_VALUE) {

    bknd_close(mDevHandle, mFileHandle);
    //wait for disconnection before a reattempt to open
    Sleep(10);
    mDevHandle = INVALID_HANDLE_VALUE;
  }

  return true;
}

//---------------------------------------------------------------------------------------------


/*Register Description accessed through the lsi 256 32bit regs
  0x00 COOKIE must read allways "TRTG"
  0x01 VERSION NR
  0x02 Detected word clock refs(16)/cycles(16) with 48mhz as ref so
       a perfect 48k sampling rate should count 0x0000BB80
  0x03 Debug register 
  0x04  b3: padding | b2: fifo depth | b1: nr_samples | b0(ins):ins  | b0(4):outs
  0x05 header filling(16bit)

  0x08 i/o matrix mapping
*/



bool CypressDevice::IsRunning() {
  return mExitHandle != INVALID_HANDLE_VALUE;
}

bool CypressDevice::IsPresent() {
  return mDevHandle != INVALID_HANDLE_VALUE;
}


#define SNAP_TOLERANCE 100
#define SNAP_TO_AND_RET(val,snapto) { if(val > (snapto - SNAP_TOLERANCE) && val < (snapto + SNAP_TOLERANCE)) return snapto; }

constexpr auto ConvertSampleRate(uint16_t srReg)
{
  uint32_t sr = srReg * 10;

  SNAP_TO_AND_RET(sr, 44100);
  SNAP_TO_AND_RET(sr, 48000);
  SNAP_TO_AND_RET(sr, 88200);
  SNAP_TO_AND_RET(sr, 96000);

  return 0;
}

static const uint16_t Sine48[48] =
{ 32768, 37045, 41248, 45307, 49151, 52715, 55938, 58764,
61145, 63041, 64418, 65255, 65535, 65255, 64418, 63041,
61145, 58764, 55938, 52715, 49151, 45307, 41248, 37045,
32768, 28490, 24287, 20228, 16384, 12820, 9597, 6771,
4390, 2494, 1117, 280, 0, 280, 1117, 2494,
4390, 6771, 9597, 12820, 16384, 20228, 24287, 28490 };

//---------------------------------------------------------------------------------------------
static const uint32_t rxpktSize = 1024;
static const uint32_t rxpktCount = 16;
static const uint32_t IsoSize = rxpktCount * rxpktSize;

//must be a power of two
static const uint8_t NrXfers = 2;
inline void NextXfer(uint8_t& val) { ++val &= (NrXfers - 1); }
static const uint8_t NrASIOBuffs = 16;
inline void NextASIO(uint8_t& val) { ++val &= (NrASIOBuffs - 1); }

//---------------------------------------------------------------------------------------------
static const bool AudioOut = true;
static const bool AudioIn  = true;

void CypressDevice::main()
{
  LOG0("CypressDevice::main");
  if (mDevHandle == INVALID_HANDLE_VALUE)
    //signal the parent of the thread failure
    return;

  bool ErrorBreak = false;
  mExitHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
  ResetEvent(mExitHandle);  

  DWORD proAudioIndex = 0;
  HANDLE AvrtHandle = AvSetMmThreadCharacteristics(L"Pro Audio", &proAudioIndex);
  AvSetMmThreadPriority(AvrtHandle, AVRT_PRIORITY_CRITICAL);


  const uint32_t nrIns = (devParams[NrIns].val + 1) * 2;
  const uint32_t nrOuts = (devParams[NrOuts].val + 1) * 2;
  nrSamples = devParams[NrSamples].val;
  const uint32_t fifoDepth = devParams[FifoDepth].val;

  InStride = nrIns * 3;
  INBuffSize = (InStride * nrSamples);

  OUTStride = (nrOuts * 3);
  OUTBuffSize = OUTStride * nrSamples;

  ZeroMemory(&mDevStatus, sizeof(mDevStatus));

  // configure the fpga channel params
  union {
    struct { uint32_t
      outs : 8,
       ins : 8,
      fifo : 8,
   padding : 8;
    };
    uint32_t u32;
  } ch_params = {
    nrOuts, nrIns, fifoDepth, 0
  };

  uint8_t* mINBuff = nullptr, * mOUTBuff = nullptr;
  devClient.AllocBuffers(INBuffSize * 2, mINBuff, OUTBuffSize * 2, mOUTBuff);

  uint8_t* inPtr[NrASIOBuffs];
  uint8_t* outPtr[NrASIOBuffs];
  asioInPtr = inPtr;
  asioOutPtr = outPtr;
  for (uint32_t c = 0; c < NrASIOBuffs; ++c)
  {
    inPtr[c] = mINBuff + (c* INBuffSize);
    outPtr[c] = mOUTBuff + (c * OUTBuffSize);
  }

  XferReq RxRequests[NrXfers];
  XferReq TxRequests[NrXfers];
  ZeroMemory(RxRequests, sizeof(RxRequests));
  ZeroMemory(TxRequests, sizeof(TxRequests));
  mRxRequests = RxRequests;
  mTxRequests = TxRequests;

  bknd_select_alt_ifc(mDevHandle, 0);
  Sleep(100);
  bknd_select_alt_ifc(mDevHandle, 3);
  //INIT the buffers based on the active alternate setting
  for (uint32_t i = 0; i < NrXfers; ++i)
  {
    if (AudioOut) {
      mTxRequests[i].handle = mDevHandle;
      mTxRequests[i].endpoint = mDefOutEP;
      mTxRequests[i].bufflen = IsoSize;

      if (bknd_init_write_xfer(mDevHandle, &mTxRequests[i], rxpktCount, rxpktSize)) {
        mTxRequests[i].ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
        ZeroMemory(mTxRequests[i].buff, IsoSize);

        for (uint8_t c = 0; c < rxpktCount; ++c)
          *(uint32_t*)(mTxRequests[i].buff + (rxpktSize * c)) = 0xaa5555aa;
      }
    }

    if (AudioIn) {
      mRxRequests[i].handle = mDevHandle;
      mRxRequests[i].endpoint = mDefInEP;
      mRxRequests[i].bufflen = IsoSize;

      if (bknd_init_read_xfer(mDevHandle, &mRxRequests[i], rxpktCount, rxpktSize)) {
        mRxRequests[i].ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
      }
    }
  }

  /*The shared mem is a single linear space where we put samples for the asio client
    and where the asio client puts output samples.
    The rxptr is the start point where isoch can write the input data
    The asioptr is the sample point for the asio client
    The txptr is the sample point where we have audio samples for the output data

    |-------|-------------|-------------------|----------------|
    0     txptr         asioptr             rxptr             len

    Each completion will process and advance the pointers until they wrap around.
    To keep the communication with the asio client simple, no buffer to asio will cross the wrap around boundary
  */
  RxProgress = 0;
  RxBuff = 0;
  AsioBuff = 0;
  TxBuff = 0;
  TxBuffPos = 0;
  IsoTxSamples = 0;
  ClientActive = false;

  mTxReqIdx = 0;
  mRxReqIdx = 0;
  mASIOHandle = devClient.GetSwitchHandle();
  ResetEvent(mASIOHandle);

  ZeroMemory(&mXferEp0Status, sizeof(mXferEp0Status));
  mXferEp0Status.handle = mDevHandle;
  mXferEp0Status.bmRequestType = 0xc0;
  mXferEp0Status.bRequest = LSI8_READ;
  mXferEp0Status.wValue = 0;
  mXferEp0Status.wIndex = 2;
  mXferEp0Status.buff = mEp0Status;
  mXferEp0Status.bufflen = 4;
  mXferEp0Status.ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);


  HANDLE timerH = CreateWaitableTimer(NULL, FALSE, nullptr);
  LARGE_INTEGER li;
  li.QuadPart = -2500000;//in 200ms
  SetWaitableTimer(timerH, &li, 250, NULL, NULL, false);

  uint32_t status = 0;
  status = ztex_xlabs_init_fifos(mDevHandle);
  status = ztex_default_lsi_set1(mDevHandle, 4, ch_params.u32);

  for (uint32_t i = 0; i < NrXfers; ++i)
  {
    if (AudioOut)
      bknd_iso_write(&mTxRequests[i]);

    if (AudioIn)
      bknd_iso_read(&mRxRequests[i]);
  }

  mDevStatus.LastSR = -1;

  while (WaitForSingleObject(mExitHandle, 0) == WAIT_TIMEOUT)
  {

    HANDLE events[] = {
      timerH,
      mXferEp0Status.ovlp.hEvent,
      mASIOHandle,
      mRxRequests[mRxReqIdx].ovlp.hEvent,
      mTxRequests[mTxReqIdx].ovlp.hEvent
    };
    uint8_t event_count = 2 + (AudioIn || AudioOut? 1:0) + (AudioIn ? 1: 0) + (AudioOut? 1:0);
    DWORD wfmo = WaitForMultipleObjects( event_count, events, false, 200);

    switch (wfmo)
    {
    case WAIT_OBJECT_0    : TimerCB();      break;// 1/4 sec timer 
    case WAIT_OBJECT_0 + 1: Ep0StatusCB();  break;
    case WAIT_OBJECT_0 + 2: AsioClientCB(); break;//ASIO ready
    case WAIT_OBJECT_0 + 3: RxIsochCB();    break;//rx isoch
    case WAIT_OBJECT_0 + 4: TxIsochCB();    break;//tx iso

    default: //not good
      if (wfmo == 0xffffffff)
      {
        LOGN("Wait error 0x%08X GetLastError:0x%08X\n", wfmo, GetLastError());
        Sleep(100);//otherwise we will block the universe
      }
      else
      {
        LOGN("Wait failed 0x%08X\n", wfmo);
        ErrorBreak = true;
        SetEvent(mExitHandle);
      }
      break;
    };
  }

  CancelWaitableTimer(timerH);
  CloseHandle(timerH);

  CloseHandle(mXferEp0Status.ovlp.hEvent);

  devClient.FreeBuffers(mINBuff, mOUTBuff);

  if (ErrorBreak)
  {
    bknd_abort_pipe(mDevHandle, mDefOutEP);
    bknd_abort_pipe(mDevHandle, mDefInEP);
  }

  for (uint32_t c = 0; c < NrXfers; ++c){
    if (AudioIn) {
      WaitForSingleObject(mRxRequests[c].ovlp.hEvent, 500);
      CloseHandle(mRxRequests[c].ovlp.hEvent);
      bknd_xfer_cleanup(&mRxRequests[c]);
    }
    if (AudioOut) {
      WaitForSingleObject(mTxRequests[c].ovlp.hEvent, 500);
      CloseHandle(mTxRequests[c].ovlp.hEvent);
      bknd_xfer_cleanup(&mTxRequests[c]);
    }
  }

  bknd_select_alt_ifc(mDevHandle, 0); //release the bandwidth

  AvRevertMmThreadCharacteristics(AvrtHandle);

  CloseHandle(mExitHandle);
  mExitHandle = INVALID_HANDLE_VALUE;
  LOG0("CypressDevice::main Exit");
}


//---------------------------------------------------------------------------------------------

void CypressDevice::UpdateClient()
{
  devClient.Switch(0, InStride, asioInPtr[AsioBuff], OUTStride, asioOutPtr[AsioBuff]);
}

//---------------------------------------------------------------------------------------------
static uint32_t sSampleCounter = 0;

static uint8_t spp[rxpktCount] = { 0 };
static uint8_t spNr = 0;

//traverses the spp array copying samples from the asio buff to the microframes based on the received distribution
uint8_t DistributeSamples( uint8_t* uf_ptrs[rxpktCount] , uint8_t*src, uint8_t NrSamples, uint32_t OUTStride)
{
  uint8_t progress = NrSamples;
  while( spNr < rxpktCount && NrSamples > 0 )
  {
    if (spp[spNr])
    {
      uint8_t samples = min(NrSamples, spp[spNr]);
      uint32_t bytes = samples * OUTStride;
      memcpy(uf_ptrs[spNr], src, bytes);
      src += bytes;
      NrSamples -= samples;
      spp[spNr] -= samples;

      uf_ptrs[spNr] += bytes;
      if (spp[spNr] == 0)
      {
        //on winusb we cannot control the length of the uframes (crAPI), so a normal 5/6/5/6/5/6 payload will have a whole trailing space.
        //Put an end mark so the fpga stops, the uframe timer will rearm.
        *((uint32_t*)(uf_ptrs[spNr])) = 0xaa5555aa;
        spNr++;
      }
    }
    else
      spNr++;
  }
  return progress - NrSamples;
}

void CypressDevice::TxIsochCB()
{
        XferReq& TxReq = mTxRequests[mTxReqIdx];
        uint8_t* ptr = TxReq.buff;
        ZeroMemory(ptr, IsoSize);

        uint32_t TxSamples = min(IsoTxSamples, IsoSize / OUTStride);
        // distribute the samples on the microframes based on the last received isoch xfer, implicit feedback

        spNr = 0;
        uint8_t* spp_ptr[rxpktCount] = { 0 };
        for (uint8_t c = 0; c < rxpktCount; ++c)
          spp_ptr[c] = ptr + (rxpktSize * c);// +(spp[c] * OUTStride);

        //partial TxBuff
        if (TxSamples > 0 && TxBuffPos > 0 && TxBuff != AsioBuff)
        {
          uint32_t count = min(nrSamples - TxBuffPos, TxSamples);

          TxBuffPos += DistributeSamples(spp_ptr, asioOutPtr[TxBuff] + (TxBuffPos * OUTStride), count, OUTStride);

          ASSERT(TxBuffPos <= nrSamples);

          if (TxBuffPos >= nrSamples)
          {
            TxBuffPos = 0;
            NextASIO(TxBuff);
          }
          TxSamples -= count;
          IsoTxSamples -= count;
        }

        //whole TxBuff

        while (TxSamples >= nrSamples && TxBuff != AsioBuff)
        {
          DistributeSamples(spp_ptr, asioOutPtr[TxBuff], nrSamples, OUTStride);

          NextASIO(TxBuff);
          TxSamples -= nrSamples;
          IsoTxSamples -= nrSamples;
        }

        //Partial TxBuff
        if (TxSamples && TxBuff != AsioBuff)
        {
          uint32_t count = min(nrSamples - TxBuffPos, TxSamples);
          TxBuffPos += DistributeSamples(spp_ptr, asioOutPtr[TxBuff] + (TxBuffPos * OUTStride), count, OUTStride);
          TxSamples -= count;
          IsoTxSamples -= count;
        }

        // silence samples
        if (TxSamples && (!ClientActive || TxBuff == AsioBuff))
        {
          for (; spNr < rxpktCount; spNr++) {
            uint32_t bytes = spp[spNr] * OUTStride;
            ZeroMemory(spp_ptr[spNr], bytes);
            IsoTxSamples -= spp[spNr];
            //mark the end of frame
            *((uint32_t*)(spp_ptr[spNr]+bytes)) = 0xaa5555aa;
          }
        }

        //ASSERT(IsoTxSamples == 0);

        //zero the rest of the buffer
        //ZeroMemory(ptr, (TxReq.buff + IsoSize) - ptr);

        bknd_iso_write(&TxReq);
        NextXfer(mTxReqIdx);
}

//---------------------------------------------------------------------------------------------

void CypressDevice::TimerCB()
{
  static uint8_t count = 0;
  //LOGN(" %u Samples/sec\r", sSampleCounter);
  if (++count == 4)
  {
    count = 0;
    mDevStatus.SwSR = sSampleCounter;
    sSampleCounter = 0;
  }

  control_xfer(mXferEp0Status);


  //mDevStatus.OutSkipCount = hdr->OutSkipCount;
  //mDevStatus.InFullCount = hdr->InFullCount;
  /*LOGN("lsi 0x%02x, 0x%08x\n", 0x20, (uint32_t)get_result);
  uint8_t flags = (uint8_t)get_result;
  for (uint8_t c = 0; c < 7; c++) {
    if (flags & 1)
    {
      get_result = ztex_default_lsi_get1(mDevHandle, 33 + c);
      LOGN("lsi 0x%02x, 0x%08x\n", 0x21 + c, (uint32_t)get_result);
    }
    flags >>= 1;
  } */
}

void CypressDevice::Ep0StatusCB()
{
  struct _t
  {
    uint16_t sr;
    uint16_t fifo;
  } & s = *(struct _t*)mEp0Status;

  uint32_t SR = ConvertSampleRate( s.sr );
  if (mDevStatus.LastSR != SR && mDevStatus.LastSR != -1 && mDevStatus.LastSR != 0)
  {
    devClient.SampleRateChanged();
  }
  mDevStatus.LastSR = SR;
  mDevStatus.FifoLevel = s.fifo;
}

//---------------------------------------------------------------------------------------------

void CypressDevice::RxIsochCB()
{
  XferReq& RxReq = mRxRequests[mRxReqIdx];
  for (uint32_t i = 0; i < rxpktCount; i++)
  {
    IsoReqResult result = bknd_iso_get_result(&RxReq, i);
    if (result.status == 0 && result.length > 0) //a filled block
    {
      uint8_t* ptr = RxReq.buff + (i * rxpktSize);
      uint32_t len = (uint32_t)result.length;
      uint32_t samples = len / InStride;
      //uint16_t residual = len % InStride;
      //ASSERT(residual == 0);
      spp[i] = samples;
      IsoTxSamples += samples;

      sSampleCounter += samples;

      if ((len + RxProgress) <= INBuffSize) {
        memcpy(asioInPtr[RxBuff] + RxProgress, ptr, len);
        RxProgress += len;
        len = 0;
      }
      else {
        memcpy(asioInPtr[RxBuff] + RxProgress, ptr, INBuffSize - RxProgress);
        len -= INBuffSize - RxProgress;
        ptr += INBuffSize - RxProgress;
        RxProgress = INBuffSize;
      }

      if (RxProgress == INBuffSize) {//Packet complete, dispatch to client
        uint8_t next = RxBuff;
        NextASIO(next);
        if (ClientActive) {

          if (RxBuff == AsioBuff)
            UpdateClient();

          if (AudioOut == false || next != TxBuff)
            RxBuff = next;
          else
          {
            ClientActive = devClient.ClientPresent();
            LOG0("ASIO queue full!");
          }
        }
        else
        {//just keep filling new ones 
          AsioBuff = RxBuff;
          TxBuff = RxBuff;
          RxBuff = next;
          TxBuffPos = 0;
          RxProgress = 0;
        }

        RxProgress = len;
        memcpy(asioInPtr[RxBuff], ptr, len);
      }

      if (RxProgress > INBuffSize) //missed something, start again
      {
        mDevStatus.ResyncErrors++;
        LOGN("Missed %u ISOCH packets\n", mDevStatus.ResyncErrors);
        RxProgress = 0;
      }
    }
  }
  //fire again
  bknd_iso_read(&RxReq);
  NextXfer(mRxReqIdx);
}

//---------------------------------------------------------------------------------------------

void CypressDevice::AsioClientCB()
{
        bool present = devClient.ClientPresent();
        if (present) {

          if (ClientActive)
            NextASIO(AsioBuff);

          if (!ClientActive || AsioBuff != RxBuff)
            UpdateClient();
        }
        else
        {
          AsioBuff = RxBuff;
          TxBuff = RxBuff;
          TxBuffPos = 0;
        }

        ClientActive = present;
}

//---------------------------------------------------------------------------------------------

bool CypressDevice::GetStatus(UsbDeviceStatus & status)
{
  if (mDevHandle != INVALID_HANDLE_VALUE) {
    if (mExitHandle != INVALID_HANDLE_VALUE) {
      status = mDevStatus;
    } else {
      int64_t result = ztex_default_lsi_get1(mDevHandle, 2);
      if (result < 0) {
        return false;
      } else {
        ZeroMemory(&status, sizeof(UsbDeviceStatus));
        status.LastSR = (uint32_t)result;
      }
    }
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------------------

uint32_t CypressDevice::GetSampleRate()
{
  uint32_t lastSR = (uint32_t)(-1);
  if (mDevHandle != INVALID_HANDLE_VALUE) {
    if (mExitHandle != INVALID_HANDLE_VALUE) {
      lastSR = mDevStatus.LastSR;
    } else {
      int64_t result = ztex_default_lsi_get1(mDevHandle, 2);
      if (result > 0)
        lastSR = ConvertSampleRate((uint32_t)result);
    }
  }
  return lastSR;
}

