
// AudioXtreamer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "AudioXtreamer.h"
#include "fx2lp\cypressdevice.h"

#include "MainFrame.h"
#include "resource.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CAudioXtreamerApp theApp;


// CAudioXtreamerApp construction
CAudioXtreamerApp::CAudioXtreamerApp()
: hMapFile(NULL)
, pBuf(nullptr)
, hAsioEvent(NULL)
, hXtreamerEvent(NULL)
, mDevice(new CypressDevice(*this))
, mClientActive(false)
{
}

BOOL CAudioXtreamerApp::InitInstance()
{
  CWinAppEx::InitInstance();

  hAsioEvent = CreateEvent(NULL, FALSE, TRUE, szNameAsioEvent);

  if (ERROR_ALREADY_EXISTS == GetLastError())
  {
    MessageBox(0, _T("Only one instance allowed!"), szNameApp, MB_ICONINFORMATION);
    return FALSE;
  }

  ResetEvent(hAsioEvent);

  hXtreamerEvent = CreateEvent(NULL, FALSE, TRUE, szNameXtreamerEvent);
  ResetEvent(hXtreamerEvent);

  //Create the named shared memory mapping and synchronization events for IPC

  hMapFile = CreateFileMapping(
    INVALID_HANDLE_VALUE,    // use paging file
    NULL,                    // default security
    PAGE_READWRITE,          // read/write access
    0,                       // maximum object size (high-order DWORD)
    (4 << SH_MEM_BLK_SIZE_SHIFT),  // maximum object size (low-order DWORD)
    szNameShMem);            // name of mapping object

  if (hMapFile == NULL) {
    _tprintf(TEXT("Could not create file mapping object (%d).\n"), GetLastError());
    return FALSE;
  }

  pBuf = (uint8_t*)MapViewOfFile(hMapFile,   // handle to map object
    FILE_MAP_ALL_ACCESS, // read/write permission
    0,
    0,
    0);

  if (pBuf == NULL) {
    _tprintf(TEXT("Could not map view of file (%d).\n"), GetLastError());
    CloseHandle(hMapFile);

    return FALSE;
  }

  pMainFrame = new MainFrame(*mDevice);
  if (pMainFrame == nullptr || !pMainFrame->Create(szNameClass, szNameApp))
    return FALSE;

  m_pMainWnd = pMainFrame;
  pMainFrame->ShowWindow(SW_HIDE);

#ifdef _DEBUG
  pMainFrame->PostMessage(WM_COMMAND, ID_AUDIOXTREAMER_OPEN);
#endif

  return TRUE;
}

//Callback from the USB driver
//we dont process the data here, just signal the client that the data is ready in the shared memory
//pass the strides and offsets to the shared struct.
// signal the xtreamer event and leave
// later we will wait for completion

bool CAudioXtreamerApp::Switch(uint32_t timeout, uint32_t rxSampleSize, uint8_t * rxBuff, uint32_t txSampleSize, uint8_t * txBuff)
{
  volatile ASIOSettings::StreamInfo *info = (ASIOSettings::StreamInfo *)pBuf;

  info->Flags &= ~((uint32_t)0x2);

  info->RxStride = rxSampleSize;
  info->RxOffset = (uint32_t)(rxBuff - pBuf) - (1 << SH_MEM_BLK_SIZE_SHIFT);
  info->TxStride = txSampleSize;
  info->TxOffset = (uint32_t)(txBuff - pBuf) - (2 << SH_MEM_BLK_SIZE_SHIFT);

  if (SetEvent(hXtreamerEvent) == FALSE)
  {
    DWORD error = GetLastError();
    _tprintf(TEXT("Could not release Mutex (%d).\n"), error);
  }


  return mClientActive;
}

bool CAudioXtreamerApp::ClientPresent()
{
  volatile ASIOSettings::StreamInfo* info = (ASIOSettings::StreamInfo*)pBuf;
  mClientActive = info->Flags & (uint32_t)0x1 ? true : false;
  info->Flags &= ~(uint32_t)0x1;
  return mClientActive;
}

void CAudioXtreamerApp::AllocBuffers(uint32_t rxSize, uint8_t *& rxBuff, uint32_t txSize, uint8_t *& txBuff)
{
  ZeroMemory(pBuf + (1 << SH_MEM_BLK_SIZE_SHIFT), (2 << SH_MEM_BLK_SIZE_SHIFT));
  rxBuff = (uint8_t*)(pBuf + (1 << SH_MEM_BLK_SIZE_SHIFT));
  txBuff = (uint8_t*)(pBuf + (2 << SH_MEM_BLK_SIZE_SHIFT));
  mClientActive = false;
}

void CAudioXtreamerApp::FreeBuffers(uint8_t *& rxBuff, uint8_t *& txBuff)
{
  rxBuff = nullptr;
  txBuff = nullptr;
}


void CAudioXtreamerApp::DeviceStopped(bool error)
{
  if (error) {
    LOG0("CAudioXtreamerApp::DeviceStopped with error");
  }
}

void CAudioXtreamerApp::SampleRateChanged()
{
  LOG0("CAudioXtreamerApp::SampleRateChanged");
  ASIOSettings::StreamInfo *info = (ASIOSettings::StreamInfo *)pBuf;
  info->Flags |= ((uint32_t)0x2);
}

int CAudioXtreamerApp::ExitInstance()
{
  if (m_pMainWnd)
    m_pMainWnd->DestroyWindow(); //and deleted

  pMainFrame = nullptr;

  if (mDevice != nullptr) {
    mDevice->Close();
    delete mDevice;
    mDevice = nullptr;
  }

  if (hAsioEvent != NULL) {
    CloseHandle(hAsioEvent);
    hAsioEvent = NULL;
  }

  if (hXtreamerEvent != NULL) {
    CloseHandle(hXtreamerEvent);
    hXtreamerEvent = NULL;
  }

  if(pBuf && !UnmapViewOfFile(pBuf))
  {
    _tprintf(TEXT("Could not Unmap view of file (%d).\n"), GetLastError());
  }
  pBuf = nullptr;
  
  if (hMapFile != NULL) {
    CloseHandle(hMapFile);
    hMapFile = NULL;
  }

  return CWinAppEx::ExitInstance();
}

void CAudioXtreamerApp::UpdateStreamParams()
{
  if (pBuf != nullptr) {
    volatile ASIOSettings::StreamInfo* info = (ASIOSettings::StreamInfo*)pBuf;
    info->NrIns = theSettings[ASIOSettings::NrIns].val;
    info->NrOuts = theSettings[ASIOSettings::NrOuts].val;
    info->NrSamples = theSettings[ASIOSettings::NrSamples].val;
    info->FifoDepth = theSettings[ASIOSettings::FifoDepth].val;
  }
}



