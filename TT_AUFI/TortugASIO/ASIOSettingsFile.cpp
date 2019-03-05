#include "stdafx.h"
#include "ASIOSettingsFile.h"

#include <simpleini\SimpleIni.h>

using namespace ASIOSettings;

TCHAR dirName[256] = { 0 };
TCHAR fileName[256] = { 0 };

ASIOSettingsFile::ASIOSettingsFile(ASIOSettings::Settings &info)
  : mInfo(info)
{
  TCHAR * env = _wgetenv(_T("APPDATA"));
  if (env)
  {
    _stprintf_s(dirName, _T("%s\\AudioXtreamer"), env);
    _stprintf_s(fileName, _T("%s\\AudioXtreamer\\ASIOSettings.ini"), env);
  }
}

ASIOSettingsFile::~ASIOSettingsFile()
{
}

const LPCTSTR defDev = _T("DefaultDevice");




BOOL DirectoryExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
    (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool ASIOSettingsFile::Open()
{
  SI_Error result = mIni.LoadFile(fileName);
  if (result != SI_OK)
  { //create default
    for (int c = 0; c < MaxSetting; ++c) {
      mIni.SetLongValue(defDev, mInfo[c].key, mInfo[c].def, mInfo[c].desc);
      mInfo[c].val = mInfo[c].def;
    }

    if ( DirectoryExists(dirName) == TRUE ||
      CreateDirectory(dirName, nullptr ) != 0  )
      result =  mIni.SaveFile(fileName);
    else
    {
      DWORD error = GetLastError();
      TCHAR msg[256];
      _stprintf(msg, _T("Error 0x%08X creating directory"), error);
      MessageBox(0, msg, _T("TortugASIO"), MB_OK);
    }
  }
  else
  {
    for (int c = 0; c < MaxSetting; ++c) {
      mInfo[c].val = mIni.GetLongValue(defDev, mInfo[c].key, mInfo[c].def, nullptr);
    }
  }
  return result == SI_OK;
}

bool ASIOSettingsFile::Save()
{
  for (int c = 0; c < MaxSetting; ++c)
    mIni.SetLongValue(defDev, mInfo[c].key, mInfo[c].val, mInfo[c].desc);

  SI_Error result = mIni.SaveFile(fileName);
  return result == SI_OK;
}
