/*
Module : NTray.h
Purpose: Interface for a C++ class to encapsulate the Shell_NotifyIcon API.
Created: PJN / 14-05-1997

Copyright (c) 1997 - 2018 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code. 

*/


/////////////////////////// Macros / Defines ///////////////////////////

#pragma once

#ifndef _NTRAY_H__
#define _NTRAY_H__

#ifndef CTRAYNOTIFYICON_EXT_CLASS
#define CTRAYNOTIFYICON_EXT_CLASS
#endif //#ifndef CTRAYNOTIFYICON_EXT_CLASS

#ifndef __ATLWIN_H__
#pragma message("CTrayNotifyIcon as of v1.51 requires ATL support to implement its functionality. If your project is MFC only, then you need to update it to include ATL support")
#endif //#ifndef __ATLWIN_H__

#ifndef _AFX
#ifndef __ATLMISC_H__
#pragma message("To avoid this message, please put atlmisc.h (part of WTL) in your pre compiled header (normally stdafx.h)")
#include <atlmisc.h> //If you do want to use CTrayNotifyIcon independent of MFC, then you need to download and install WTL from http://sourceforge.net/projects/wtl
#endif //#ifndef __ATLMISC_H__
#endif //#ifndef _AFX


/////////////////////////// Classes ///////////////////////////////////////////

//the actual tray notification class wrapper
class CTRAYNOTIFYICON_EXT_CLASS CTrayNotifyIcon : public ATL::CWindowImpl<CTrayNotifyIcon>
{
public:
//Enums / Typedefs
 #ifndef _AFX
  typedef _CSTRING_NS::CString String;
#else
  typedef CString String;
#endif //#ifndef _AFX

  enum BalloonStyle
  {
    Warning,
    Error,
    Info,
    None,
    User
  };

  //We use our own definitions of the NOTIFYICONDATA struct so that we can use all the
  //functionality without requiring client code to define _WIN32_IE >= 0x500
  struct NOTIFYICONDATA_4
  {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    TCHAR szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    TCHAR szInfo[256];
    union 
    {
      UINT uTimeout;
      UINT uVersion;
    } DUMMYUNIONNAME;
    TCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
    HICON hBalloonIcon;
  };

#pragma warning(suppress: 26440 26477)
  DECLARE_WND_CLASS(_T("TrayNotifyIconClass"))

//Constructors / Destructors
  CTrayNotifyIcon() noexcept;
  CTrayNotifyIcon(_In_ const CTrayNotifyIcon&) = delete;
  CTrayNotifyIcon(_In_ CTrayNotifyIcon&&) = delete;
  ~CTrayNotifyIcon();
  CTrayNotifyIcon& operator=(_In_ const CTrayNotifyIcon&) = delete;
  CTrayNotifyIcon& operator=(_In_ CTrayNotifyIcon&&) = delete;

//Create the tray icon
#ifdef _AFX
  bool Create(_In_ CWnd* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ HICON hIcon, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bShow = true);
  bool Create(_In_ CWnd* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ const CBitmap* pBitmap, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bShow = true);
  bool Create(_In_ CWnd* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ HICON* phIcons, _In_ int nNumIcons, _In_ DWORD dwDelay, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bShow = true);
  bool Create(_In_ CWnd* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ UINT nTimeout, _In_ BalloonStyle style, _In_ HICON hIcon, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_opt_ HICON hBalloonIcon = nullptr, _In_ bool bQuietTime = false, _In_ bool bShow = true);
  bool Create(_In_ CWnd* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ UINT nTimeout, _In_ BalloonStyle style, _In_ const CBitmap* pBitmap, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_opt_ HICON hBalloonIcon = nullptr, _In_ bool bQuietTime = false, _In_ bool bShow = true);
  bool Create(_In_ CWnd* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ UINT nTimeout, _In_ BalloonStyle style, _In_ HICON* phIcons, _In_ int nNumIcons, _In_ DWORD dwDelay, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_opt_ HICON hBalloonIcon = nullptr, _In_ bool bQuietTime = false, _In_ bool bShow = true);
#else
  bool Create(_In_ CWindow* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ HICON hIcon, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bShow = true);
  bool Create(_In_ CWindow* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ const CBitmap* pBitmap, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bShow = true);
  bool Create(_In_ CWindow* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ HICON* phIcons, _In_ int nNumIcons, _In_ DWORD dwDelay, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bShow = true);
  bool Create(_In_ CWindow* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ UINT nTimeout, _In_ BalloonStyle style, _In_ HICON hIcon, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_opt_ HICON hBalloonIcon = nullptr, _In_ bool bQuietTime = false, _In_ bool bShow = true);
  bool Create(_In_ CWindow* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ UINT nTimeout, _In_ BalloonStyle style, _In_ const CBitmap* pBitmap, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_opt_ HICON hBalloonIcon = nullptr, _In_ bool bQuietTime = false, _In_ bool bShow = true);
  bool Create(_In_ CWindow* pNotifyWnd, _In_ UINT uID, _In_ LPCTSTR pszTooltipText, _In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ UINT nTimeout, _In_ BalloonStyle style, _In_ HICON* phIcons, _In_ int nNumIcons, _In_ DWORD dwDelay, _In_ UINT nNotifyMessage, _In_ UINT uMenuID = 0, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_opt_ HICON hBalloonIcon = nullptr, _In_ bool bQuietTime = false, _In_ bool bShow = true);
#endif //#ifdef _AFX

//Sets or Gets the tooltip text
  bool   SetTooltipText(_In_ LPCTSTR pszTooltipText) noexcept;
  bool   SetTooltipText(_In_ UINT nID);
  String GetTooltipText() const;

//Sets or Gets the icon displayed
  bool SetIcon(_In_ HICON hIcon) noexcept;
  bool SetIcon(_In_ const CBitmap* pBitmap);
  bool SetIcon(_In_ LPCTSTR lpIconName);
  bool SetIcon(_In_ UINT nIDResource);
  bool SetIcon(_In_ HICON* phIcons, _In_ int nNumIcons, _In_ DWORD dwDelay) noexcept;
  bool SetStandardIcon(_In_ LPCTSTR lpIconName) noexcept;
  bool SetStandardIcon(_In_ UINT nIDResource) noexcept;
  HICON GetIcon() const noexcept;
  bool UsingAnimatedIcon() const noexcept;

//Sets or Gets the window to send notification messages to
#ifdef _AFX
  bool SetNotificationWnd(_In_ CWnd* pNotifyWnd) noexcept;
  CWnd* GetNotificationWnd() const noexcept;
#else
  bool SetNotificationWnd(_In_ CWindow* pNotifyWnd) noexcept;
  CWindow* GetNotificationWnd() const noexcept;
#endif //#ifdef _AFX

//Modification of the tray icons
  bool Delete(_In_ bool bCloseHelperWindow = true) noexcept;
  bool Create(_In_ bool bShow = true) noexcept;
  bool Hide() noexcept;
  bool Show() noexcept;

//Dynamic tray icon support
  HICON BitmapToIcon(_In_ const CBitmap* pBitmap);
  static bool GetDynamicDCAndBitmap(_In_ CDC* pDC, _In_ CBitmap* pBitmap);

//Modification of the menu to use as the context menu
  void SetMenu(_In_ HMENU hMenu);
  CMenu& GetMenu() noexcept;
  void SetDefaultMenuItem(_In_ UINT uItem, _In_ bool fByPos);
  void GetDefaultMenuItem(_Out_ UINT& uItem, _Out_ bool& fByPos) noexcept 
  {
    uItem = m_nDefaultMenuItem; 
    fByPos = m_bDefaultMenuItemByPos; 
  };

//Default handler for tray notification message
  virtual LRESULT OnTrayNotification(WPARAM uID, LPARAM lEvent);

//Status information
  bool IsShowing() const noexcept { return !IsHidden(); };
  bool IsHidden() const noexcept { return m_bHidden; };

//Sets or gets the Balloon style tooltip settings
  bool         SetBalloonDetails(_In_ LPCTSTR pszBalloonText, _In_ LPCTSTR pszBalloonCaption, _In_ BalloonStyle style, _In_ UINT nTimeout, _In_ HICON hUserIcon = nullptr, _In_ bool bNoSound = false, _In_ bool bLargeIcon = false, _In_ bool bRealtime = false, _In_ HICON hBalloonIcon = nullptr) noexcept;
  String       GetBalloonText() const;
  String       GetBalloonCaption() const;
  UINT         GetBalloonTimeout() const noexcept;

//Other functionality
  bool SetVersion(_In_ UINT uVersion) noexcept;
  bool SetFocus() noexcept;

//Helper methods to load tray icon from resources
  static HICON LoadIcon(_In_ LPCTSTR lpIconName, _In_ bool bLargeIcon = false);
  static HICON LoadIcon(_In_ UINT nIDResource, _In_ bool bLargeIcon = false);
  static HICON LoadIcon(_In_ HINSTANCE hInstance, _In_ LPCTSTR lpIconName, _In_ bool bLargeIcon = false) noexcept;
  static HICON LoadIcon(_In_ HINSTANCE hInstance, _In_ UINT nIDResource, _In_ bool bLargeIcon = false) noexcept;

protected:
//Methods
  bool         CreateHelperWindow();
  bool         StartAnimation(_In_ HICON* phIcons, _In_ int nNumIcons, _In_ DWORD dwDelay) noexcept;
  void         StopAnimation() noexcept;
  HICON        GetCurrentAnimationIcon() const noexcept;
  BOOL ProcessWindowMessage(_In_ HWND hWnd, _In_ UINT nMsg, _In_ WPARAM wParam, _In_ LPARAM lParam, _Inout_ LRESULT& lResult, _In_ DWORD dwMsgMapID) noexcept override;
  LRESULT      OnTaskbarCreated(WPARAM wParam, LPARAM lParam) noexcept;
  void         OnTimer(UINT_PTR nIDEvent) noexcept;
  void         OnDestroy() noexcept;

//Member variables
  NOTIFYICONDATA_4     m_NotifyIconData;
  bool                 m_bCreated;
  bool                 m_bHidden;
#ifdef _AFX
  CWnd*                m_pNotificationWnd;
#else
  CWindow*             m_pNotificationWnd;
#endif //#ifdef _AFX
  CMenu                m_Menu;
  UINT                 m_nDefaultMenuItem;
  bool                 m_bDefaultMenuItemByPos;
  HICON                m_hDynamicIcon; //Our cached copy of the last icon created with BitmapToIcon
  ATL::CHeapPtr<HICON> m_Icons;
  int                  m_nNumIcons;
  UINT_PTR             m_nTimerID;
  int                  m_nCurrentIconIndex;
};

#endif //#ifndef _NTRAY_H__
