#include "stdafx.h"
#include "PropertySheetDlg.h"



class CAboutDlg : public CDialog
{
public:
  CAboutDlg();

  // Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ABOUTBOX };
#endif

protected:
  virtual void DoDataExchange(CDataExchange * DX);    // DDX/DDV support

// Implementation
protected:

public:

};


CAboutDlg::CAboutDlg() : CDialog(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* DX)
{
  CDialog::DoDataExchange(DX);
}


// PropertySheet dialog

#include "SettingsDlg.cpp"
#include "AudioXtreamerDlg.cpp"


PropertySheetDlg::PropertySheetDlg(UsbDevice &usbdev, CWnd * parent)
: CPropertySheet(szNameApp, parent)
, mDevice(usbdev)
, pp1(new ASIOSettingsDlg( usbdev, theSettings))
, pp2(new CAudioXtreamerDlg(usbdev, theSettings))
{
  AddPage(pp1);
  AddPage(pp2);
}

PropertySheetDlg::~PropertySheetDlg()
{
  RemovePage(pp2);
  RemovePage(pp1);
  delete pp2;
  delete pp1;
}

BOOL PropertySheetDlg::OnInitDialog()
{
  BOOL bResult = CPropertySheet::OnInitDialog();

  //Menu.LoadMenu(IDR_MENU);
  mHicon = AfxGetApp()->LoadIcon(IDI_AXG);

  CMenu * sysMenu = GetSystemMenu(FALSE);
  sysMenu->InsertMenu(1, MF_BYPOSITION, IDM_ABOUTBOX, _T("&About"));
  sysMenu->InsertMenu(3, MF_BYPOSITION, 1234, _T("&Exit"));
  SetIcon(mHicon, TRUE);
  SetIcon(mHicon, FALSE);

  return bResult;
}


BEGIN_MESSAGE_MAP(PropertySheetDlg, CPropertySheet)
  ON_WM_DESTROY()
  ON_WM_SYSCOMMAND()
  ON_COMMAND(ID_APPLY_NOW, &PropertySheetDlg::OnApplyNow)
END_MESSAGE_MAP()


void PropertySheetDlg::OnDestroy()
{
  CPropertySheet::OnDestroy();
}

void PropertySheetDlg::OnApplyNow()
{
  Default();
  //compare the data of the 

}

void PropertySheetDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
  if ((nID & 0xFFF0) == IDM_ABOUTBOX)
  {
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
  }
  else
  {
    CPropertySheet::OnSysCommand(nID, lParam);
  }
}

bool PropertySheetDlg::ManualControls()
{
  return GetActiveIndex() == 1;
}
