#include "stdafx.h"
#include "SettingsDlg.h"

using namespace ASIOSettings;


ASIOSettingsDlg::ASIOSettingsDlg(UsbDevice & dev, Settings &info)
  : CPropertyPage(IDD_DLG_ASIO)
  , mDev(dev)
  , mInfo(info)
{
  memset(mSRvals, 0, sizeof(mSRvals));
  mSRacc = 0;
  mSRidx = 0;
  mLastSR = 0;
}



ASIOSettingsDlg::~ASIOSettingsDlg()
{
}


BEGIN_MESSAGE_MAP(ASIOSettingsDlg, CPropertyPage)
  ON_WM_TIMER()
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_WM_HSCROLL()
  ON_WM_SHOWWINDOW()
  ON_CBN_SELCHANGE(IDC_DL_INS, &ASIOSettingsDlg::OnCbnSelchangeChannels)
  ON_CBN_SELCHANGE(IDC_DL_OUTS, &ASIOSettingsDlg::OnCbnSelchangeChannels)
END_MESSAGE_MAP()



void ASIOSettingsDlg::OnTimer(UINT_PTR nIDEvent)
{
  TCHAR str[32];
  UsbDeviceStatus ds;
  mDev.GetStatus(ds);

  if ( nIDEvent == 200 ) {
    uint32_t sr = mDev.GetSampleRate();
    if (sr != mLastSR) {
      CDataExchange pDX(this, false);
      mLastSR = sr;
      int val = -1;
      switch (sr) {
      case 44100: val = 0; break;
      case 48000: val = 1; break;
      case 88200: val = 2; break;
      case 96000: val = 3; break;
      }
      DDX_Radio(&pDX, IDC_RADIO_44, val);
      UpdateRanges(SCBoth);
    }
    //LOGN("%u\n", ds.SwSR);
    _stprintf(str, _T("%u"), ds.SwSR);
    GetDlgItem(IDC_STATIC_SR)->SetWindowText(str);
  }

  _stprintf(str, _T("Skip: %u"), ds.OutSkipCount);
  GetDlgItem(IDC_STATIC_SKIP)->SetWindowText(str);

  _stprintf(str, _T("Full: %u"), ds.InFullCount);
  GetDlgItem(IDC_STATIC_FULL)->SetWindowText(str);  

  mProgressFifo.SetPos(ds.FifoLevel);

  __super::OnTimer(nIDEvent);
}


int ASIOSettingsDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (__super::OnCreate(lpCreateStruct) == -1)
    return -1;

  mIns = -1;
  mOuts = -1;
  mSamples = -1;
  mFifo = -1;

  return 0;
}

void ASIOSettingsDlg::OnDestroy()
{
  KillTimer(200);
  KillTimer(20);
  __super::OnDestroy();
}


inline void ASIOSettingsDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
  CSliderCtrl* pSlider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
  if (pSlider == &mSliderSamples || pSlider == &mSliderFifo) {
    UpdateRanges(pSlider == &mSliderFifo ? SCFifo : SCSamples);
    SetModified(TRUE);
  }
}

inline void ASIOSettingsDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
  if (bShow == TRUE) {
    SetTimer(200, 200, nullptr);
    SetTimer(20, 20, nullptr);
    mLastSR = 0;
  } else {
    KillTimer(200);
    KillTimer(20);
  }
}

inline void ASIOSettingsDlg::UpdateRanges(SCType type)
{
  CDataExchange pDX(this, false);
  int nrSamples = mSliderSamples.GetPos();
  int fifoDepth = mSliderFifo.GetPos();
  mProgressFifo.SetRange(0, fifoDepth);
  TCHAR str[16] = { 0 };

  float inlat = nrSamples * 1000.f / (mLastSR ? mLastSR : 1);
  float outlat = ((nrSamples + fifoDepth) * 1000.f) / (mLastSR ? mLastSR : 1);

  if (type == SCSamples || type == SCBoth) {
    DDX_Text(&pDX, IDC_STATIC_NRSAMPLES, nrSamples);
    if (mLastSR != 0) {
      _stprintf(str, _T("%1.2f ms"), inlat);
      DDX_Text(&pDX, IDC_STATIC_INLAT, str, 8);
    }
  }

  if (type == SCFifo || type == SCBoth) {
    DDX_Text(&pDX, IDC_STATIC_FIFOD, fifoDepth);
  }

  if (mLastSR != 0) {
    _stprintf(str, _T("%1.2f ms"), outlat);
    DDX_Text(&pDX, IDC_STATIC_OUTLAT, str, 8);
  }
}


BOOL ASIOSettingsDlg::OnInitDialog()
{
  __super::OnInitDialog();//will call DoDataExchange

  //populate the controls
  for (uint8_t c = 0; c < ASIOSettings::ChanEntires; ++c) {
    TCHAR str[16];
    _stprintf(str, _T("%u"), (c + 1) * 2);
    mListIns.InsertString(c, str);
    mListOuts.InsertString(c, str);
  }

  mIns    = mInfo[NrIns].val;
  mOuts   = mInfo[NrOuts].val;
  mSamples= mInfo[NrSamples].val;
  mFifo   = mInfo[FifoDepth].val;

  mListIns.SetCurSel(mIns);
  mListOuts.SetCurSel(mOuts);

  mSliderSamples.SetRangeMin(16);
  mSliderSamples.SetPos(16);
  mSliderSamples.SetRangeMax(mInfo[NrSamples].max, TRUE);
  mSliderSamples.SetPos(mSamples);

  mSliderFifo.SetRangeMin(16);
  mSliderFifo.SetPos(16);
  mSliderFifo.SetRangeMax(mInfo[FifoDepth].max, TRUE);
  mSliderFifo.SetPos(mFifo);

  SetWindowTheme(mProgressFifo.GetSafeHwnd(), L" ", L" ");

  //if inside a propertysheet, remove the ok and cancel button and show the patch editor
  if (dynamic_cast<CPropertyPage*>(this)) {
    GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
    GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
  } else {
    GetDlgItem(ID_PATCHEDIT)->ShowWindow(SW_HIDE);
  }

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void ASIOSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
  __super::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_DL_INS, mListIns);
  DDX_Control(pDX, IDC_DL_OUTS, mListOuts);
  DDX_Control(pDX, IDC_SLIDER_SAMPLES, mSliderSamples);
  DDX_Control(pDX, IDC_SLIDER_FIFO, mSliderFifo);

  DDX_CBIndex(pDX, IDC_DL_INS, mIns);
  DDX_CBIndex(pDX, IDC_DL_OUTS, mOuts);

  DDX_Slider(pDX, IDC_SLIDER_SAMPLES, mSamples);
  DDX_Slider(pDX, IDC_SLIDER_FIFO, mFifo);

  DDX_Control(pDX, IDC_PROGRESS_FIFO, mProgressFifo);

  //all controls must be subclassed by now
  if (pDX->m_bSaveAndValidate == false)
  {
    UpdateRanges(SCBoth);
  }
}


void ASIOSettingsDlg::OnCbnSelchangeChannels()
{
  SetModified(TRUE);
}

void ASIOSettingsDlg::OnOK()
{
  if (mInfo[NrIns].val != mIns || mInfo[NrOuts].val != mOuts || mInfo[NrSamples].val != mSamples || mInfo[FifoDepth].val != mFifo)
  {
    mInfo[NrIns].val = mIns;
    mInfo[NrOuts].val = mOuts;
    mInfo[NrSamples].val = mSamples;
    mInfo[FifoDepth].val = mFifo;
    mDev.Close();
  }

  CPropertyPage::OnOK();
}
