#include "stdafx.h"
#include "SettingsDlg.h"
#include "resource.h"

using namespace ASIOSettings;

ASIOSettingsDlg::ASIOSettingsDlg(UsbDevice & dev, Settings &info)
  : CDialog(IDD_DLG_ASIO)
  , mDev(dev)
  , mInfo(info)
{
  memset(mSRvals, 0, sizeof(mSRvals));
  mSRacc = 0;
  mSRidx = 0;
}


ASIOSettingsDlg::~ASIOSettingsDlg()
{
}
BEGIN_MESSAGE_MAP(ASIOSettingsDlg, CDialog)
  ON_WM_TIMER()
  ON_WM_CREATE()
  ON_WM_DESTROY()
END_MESSAGE_MAP()


void ASIOSettingsDlg::OnTimer(UINT_PTR nIDEvent)
{
  UsbDeviceStatus devStatus;
  CWnd * wnd = GetDlgItem(IDC_STATIC_STATUS_VAR);
  if (nullptr != wnd && mDev.GetStatus(devStatus))
  {
    TCHAR str[32];
    uint16_t count = (uint16_t)(devStatus.st[2] & 0xFFFF);
    uint16_t fract = (uint16_t)(devStatus.st[2]>>16);
    uint16_t sr = count * 100;
    mSRacc -= mSRvals[mSRidx];
    mSRacc += sr;
    mSRvals[mSRidx] = sr;
    mSRidx = (mSRidx+1) & 0xf;

    _stprintf(str, _T("Online %u Khz"), sr) ;
    wnd->SetWindowText(str);
  } else
    wnd->SetWindowText(_T("Offline"));


  CDialog::OnTimer(nIDEvent);
}


int ASIOSettingsDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (CDialog::OnCreate(lpCreateStruct) == -1)
    return -1;

  SetTimer(1234, 100, nullptr);
  return 0;
}


void ASIOSettingsDlg::OnDestroy()
{
  CDialog::OnDestroy();

  KillTimer(1234);
}


BOOL ASIOSettingsDlg::OnInitDialog()
{
  CDialog::OnInitDialog();//will call DoDataExchange

  //populate the controls
  for (uint8_t c = 0; c < ASIOSettings::ChanEntires; ++c)
  {
    TCHAR str[16];
    _itow_s((c+1)*2, str, 10);
    mListIns.InsertString(c, str);
    mListOuts.InsertString(c, str);
  }

  mListIns.SetCurSel(mInfo[NrIns].val);
  mListOuts.SetCurSel(mInfo[NrOuts].val);

  int idx = 0;
  for (uint16_t c = 32; c <= 256; c += 16, idx++)
  {
    TCHAR str[16];
    _itow_s(c, str, 10);

    mListNrSamples.InsertString(idx, str);
    mListNrSamples.SetItemData(idx, c);
    if (c == mInfo[NrSamples].val)
      mListNrSamples.SetCurSel(idx);

    mListFifoDepth.InsertString(idx, str);
    mListFifoDepth.SetItemData(idx, c);
    if (c == mInfo[FifoDepth].val)
      mListFifoDepth.SetCurSel(idx);
  }

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void ASIOSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_DL_INS, mListIns);
  DDX_Control(pDX, IDC_DL_OUTS, mListOuts);
  DDX_Control(pDX, IDC_DL_NRS, mListNrSamples);
  DDX_Control(pDX, IDC_DL_FIFO, mListFifoDepth);

  DDX_CBIndex(pDX, IDC_DL_INS, mInfo[NrIns].val);
  DDX_CBIndex(pDX, IDC_DL_OUTS, mInfo[NrOuts].val);

  if (pDX && pDX->m_bSaveAndValidate)
  {
    int idx = mListNrSamples.GetCurSel();
    if (idx >= 0)
    {
      int val = (int)mListNrSamples.GetItemData(idx);
      mInfo[NrSamples].val = val > mInfo[NrSamples].max ? mInfo[NrSamples].def : val;
    }

    idx = mListFifoDepth.GetCurSel();
    if (idx >= 0)
    {
      int val = (int)mListFifoDepth.GetItemData(idx);
      mInfo[FifoDepth].val = val > mInfo[FifoDepth].max ? mInfo[FifoDepth].def : val;
    }
  }
}