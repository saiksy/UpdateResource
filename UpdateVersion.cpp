#include "stdafx.h"
#include "PEResInfo.h"
#include <locale.h>

struct VS_VERSIONINFO
{
    WORD                wLength;
    WORD                wValueLength;
    WORD                wType;
    WCHAR               szKey[1];
    WORD                wPadding1[1];
    VS_FIXEDFILEINFO    Value;
    WORD                wPadding2[1];
    WORD                wChildren[1];
};

struct
{
    WORD wLanguage;
    WORD wCodePage;
} *lpTranslate;

struct UPDATE_INFO 
{
	TCHAR szPath[MAX_PATH];
	TCHAR szFileVersion[64];
	TCHAR szProduction[64];
	BOOL bAutoIncVersion;
};

void PringUsage()
{
	printf("UpdateVersion /s c:\\windows\\notopad.exe /f 1.0.0.1 /p 1.0.0.1\n"
		"/s : file path /f : file version /p : product version\n"
		"为空则保留原有值.\n");
	exit(-1);
}

bool VerStringToInt(LPCTSTR lpVersion, DWORD VerNum[4])
{
	if (!lpVersion)
	{
		return false;
	}
	
	LPCTSTR lpToks = _tcstok((LPTSTR)lpVersion, _T("."));
	
	DWORD *pArr = VerNum;
	
	while (lpToks)
	{
		*pArr++ = wcstol(lpToks, NULL, 10);
		lpToks = _tcstok(NULL, _T("."));
	}

	return true;
}

bool ParseCmdLine(int _Argc, wchar_t ** _Argv, UPDATE_INFO &UpdateInfo)
{
	if (_Argc < 2 || !_Argv)
	{
		return false;
	}

	ZeroMemory(&UpdateInfo, sizeof(UPDATE_INFO));
	
	bool bParseSucc = false;
	wchar_t **pArg = &_Argv[1];

	for (; *pArg; pArg++)
	{
		if (lstrcmpi(*pArg, _T("/s")) == 0)
		{
			StringCchCopy(UpdateInfo.szPath, sizeof(UpdateInfo.szPath), *++pArg);
			_tprintf(_T("file path %s\n"), *pArg);
			bParseSucc = true;
		}
		else if (lstrcmpi(*pArg, _T("/f")) == 0)
		{
			StringCchCopy(UpdateInfo.szFileVersion, sizeof(UpdateInfo.szFileVersion), *++pArg);
			_tprintf(_T("file version %s\n"), *pArg);
		}
		else if (lstrcmpi(*pArg, _T("/p")) == 0)
		{
			StringCchCopy(UpdateInfo.szProduction, sizeof(UpdateInfo.szProduction), *++pArg);
			_tprintf(_T("production version %s\n"), *pArg);
		}
		else if (lstrcmpi(*pArg, _T("/a")) == 0)
		{
			UpdateInfo.bAutoIncVersion = TRUE;
		}
	}
	return bParseSucc;
}

int wmain(int _Argc, wchar_t ** _Argv)
{
	setlocale(LC_ALL, "chs");

	if (_Argc < 2)
		PringUsage();
	
	UPDATE_INFO UpdateInfo = {};
	if (!ParseCmdLine(_Argc, _Argv, UpdateInfo))
	{
		printf("Parse CmdLine failure\n");
		exit(-1);
	}
	
	bool bChangeFileVer = lstrlen(UpdateInfo.szFileVersion) > 0, 
		bChangeProductionVer = lstrlen(UpdateInfo.szProduction) > 0;
	
	CPEResInfo PEInfo;
	PEInfo.Open(UpdateInfo.szPath);
	

	DWORD FileVerNums[4] = {}, ProVerNums[4] = {};
	if (bChangeFileVer)
	{
		VerStringToInt(UpdateInfo.szFileVersion, FileVerNums);
	}
	else
	{
		CPEResInfo::GetFileVersion(UpdateInfo.szPath, &FileVerNums[0], &FileVerNums[1], &FileVerNums[2], &FileVerNums[3]);
		FileVerNums[3] += 1;
	}
	
	if (bChangeProductionVer)
	{
		VerStringToInt(UpdateInfo.szProduction, ProVerNums);
	}
	else
	{
		CPEResInfo::GetFileVersion(UpdateInfo.szPath, &ProVerNums[0], &ProVerNums[1], &ProVerNums[2], &ProVerNums[3]);
		ProVerNums[3] += 1;
	}

	bool bret = PEInfo.UpdateResVersion(FileVerNums, ProVerNums);
	PEInfo.Close(TRUE);
	printf("update version %s\n", bret ? "succ" : "failure");

	return bret ? 0 : -1;
}
