#include "stdafx.h"
#include "resource.h"
#include "SettingsDlg.h"

using namespace ASIOSettings;

template <class T>
ASIOSettingsDlg<T>::ASIOSettingsDlg(UsbDevice & dev, Settings &info)
  : T(IDD_DLG_ASIO)
  , mDev(dev)
  , mInfo(info)
{
  memset(mSRvals, 0, sizeof(mSRvals));
  mSRacc = 0;
  mSRidx = 0;
  mSRLast = 0;
}


template <class T>
ASIOSettingsDlg<T>::~ASIOSettingsDlg()
{
}


BEGIN_TEMPLATE_MESSAGE_MAP(ASIOSettingsDlg, T, T)
  ON_WM_TIMER()
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_WM_HSCROLL()
  ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

#define SNAP_TOLERANCE 300
#define SNAP_TO_AND_RET(val,snapto) { if(val > (snapto - SNAP_TOLERANCE) && val < (snapto + SNAP_TOLERANCE)) return snapto; }

uint32_t SnapSamplingRate(uint32_t val)
{
  SNAP_TO_AND_RET(val, 44100);
  SNAP_TO_AND_RET(val, 48000);
  SNAP_TO_AND_RET(val, 88100);
  SNAP_TO_AND_RET(val, 96000);
  return 0;
}

template <class T>
void ASIOSettingsDlg<T>::OnTimer(UINT_PTR nIDEvent)
{
  UsbDeviceStatus ds;
  CDataExchange pDX(this, false);
  TCHAR str[32];
  mDev.GetStatus(ds);
  if ( nIDEvent == 200 )
  {
    uint16_t count = (uint16_t)(ds.LastSR & 0xFFFF);
    uint16_t fract = (uint16_t)(ds.LastSR>>16);
    uint32_t sr = count * 100;
    mSRacc -= mSRvals[mSRidx];
    mSRacc += sr;
    mSRvals[mSRidx] = sr;
    mSRidx = (mSRidx+1) & 0xf;

    mSRLast = SnapSamplingRate(sr);
    _stprintf(str, _T("Online %u hz"), mSRLast);
    DDX_Text(&pDX, IDC_STATIC_STATUS_ONLINESR, str, 32);
    UpdateRanges(SCBoth);
  }
 
  _stprintf(str, _T("Resync Errors: %u"), ds.ResyncErrors );
  DDX_Text(&pDX, IDC_STATIC_REFRAMES, str, 32);

  _stprintf(str, _T("Skip: %u"), ds.SkipCount);
  DDX_Text(&pDX, IDC_STATIC_SKIP, str, 32);

  mProgressFifo.SetPos(ds.FifoLevel);

  T::OnTimer(nIDEvent);
}

template <class T>
int ASIOSettingsDlg<T>::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (T::OnCreate(lpCreateStruct) == -1)
    return -1;

  return 0;
}

template <class T>
void ASIOSettingsDlg<T>::OnDestroy()
{
  KillTimer(200);
  KillTimer(20);
  T::OnDestroy();
}

template<class T>
inline void ASIOSettingsDlg<T>::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
  CSliderCtrl* pSlider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
  if (pSlider == &mSliderSamples)
    UpdateRanges(SCSamples);
  else if( pSlider == &mSliderFifo)
    UpdateRanges(SCFifo);
}

template<class T>
inline void ASIOSettingsDlg<T>::OnShowWindow(BOOL bShow, UINT nStatus)
{
  if (bShow == TRUE) {
    SetTimer(200, 200, nullptr);
    SetTimer(20, 20, nullptr);
  } else {
    KillTimer(200);
    KillTimer(20);
  }
}

template<class T>
inline void ASIOSettingsDlg<T>::UpdateRanges(SCType type)
{
  CDataExchange pDX(this, false);
  int nrSamples = mSliderSamples.GetPos();
  int fifoDepth = mSliderFifo.GetPos();
  mProgressFifo.SetRange(0, fifoDepth);
  TCHAR str[16] = { 0 };

  float inlat = nrSamples * 1000.f / (mSRLast ? mSRLast : 1);
  float outlat = ((nrSamples + fifoDepth) * 1000.f) / (mSRLast ? mSRLast : 1);

  if (type == SCSamples || type == SCBoth) {
    DDX_Text(&pDX, IDC_STATIC_NRSAMPLES, nrSamples);
    if (mSRLast != 0) {
      _stprintf(str, _T("%1.2f ms"), inlat);
      DDX_Text(&pDX, IDC_STATIC_INLAT, str, 8);
    }
  }

  if (type == SCFifo || type == SCBoth) {
    DDX_Text(&pDX, IDC_STATIC_FIFOD, fifoDepth);
  }

  if (mSRLast != 0) {
    _stprintf(str, _T("%1.2f ms"), outlat);
    DDX_Text(&pDX, IDC_STATIC_OUTLAT, str, 8);
  }
}


template <class T>
BOOL ASIOSettingsDlg<T>::OnInitDialog()
{
  T::OnInitDialog();//will call DoDataExchange

  //populate the controls
  for (uint8_t c = 0; c < ASIOSettings::ChanEntires; ++c) {
    TCHAR str[16];
    _itow_s((c+1)*2, str, 10);
    mListIns.InsertString(c, str);
    mListOuts.InsertString(c, str);
  }

  mListIns.SetCurSel(mInfo[NrIns].val);
  mListOuts.SetCurSel(mInfo[NrOuts].val); 

  mSliderSamples.SetRangeMin(16);
  mSliderSamples.SetPos(mInfo[NrSamples].val);
  mSliderSamples.SetRangeMax(mInfo[NrSamples].max, TRUE);

  mSliderFifo.SetRangeMin(16);
  mSliderFifo.SetPos(mInfo[FifoDepth].val);
  mSliderFifo.SetRangeMax(mInfo[FifoDepth].max, TRUE);

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

template <class T>
void ASIOSettingsDlg<T>::DoDataExchange(CDataExchange* pDX)
{
  T::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_DL_INS, mListIns);
  DDX_Control(pDX, IDC_DL_OUTS, mListOuts);
  DDX_Control(pDX, IDC_SLIDER_SAMPLES, mSliderSamples);
  DDX_Control(pDX, IDC_SLIDER_FIFO, mSliderFifo);

  DDX_CBIndex(pDX, IDC_DL_INS, mInfo[NrIns].val);
  DDX_CBIndex(pDX, IDC_DL_OUTS, mInfo[NrOuts].val);

  DDX_Slider(pDX, IDC_SLIDER_SAMPLES, mInfo[NrSamples].val);
  DDX_Slider(pDX, IDC_SLIDER_FIFO, mInfo[FifoDepth].val);

  DDX_Control(pDX, IDC_PROGRESS_FIFO, mProgressFifo);

  //all controls must be subclassed by now
  if (pDX->m_bSaveAndValidate == false)
  {
    UpdateRanges(SCBoth);
  }
}

