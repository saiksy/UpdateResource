#include "StdAfx.h"
#include "PEResInfo.h"

#pragma comment(lib, "Version.lib")

CPEResInfo::CPEResInfo()
 : m_hExeFile(NULL)
 , m_bVerInfoModified(FALSE)
{
	ZeroMemory(m_szCurExePath, sizeof(m_szCurExePath));
	ZeroMemory(&m_verInfo, sizeof(UPDATE_VERSION_RES_INFO));
	m_wLangID = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
}

CPEResInfo::~CPEResInfo()
{
	Close(FALSE);
}

BOOL CPEResInfo::Open(LPCWSTR lpszExePath)
{
	if (NULL==lpszExePath || L'\0'==lpszExePath[0])
	{
		return FALSE;
	}
	Close(FALSE);
	WCHAR szLogDesc[MAX_PATH+20] = { 0 };
	// 先把版本信息等缓存出来，然后再BeginUpdateResource开文件
	if (FALSE==InnerGetVerInfo(lpszExePath))
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"获取%s的版本资源信息失败", lpszExePath);
	}

	// 字符串的信息
	HMODULE hMod = LoadLibraryW(lpszExePath);
	if (hMod)
	{
		// 我们目前只关心这两个字符串的信息
		DWORD dwGroupId = ResId2GroupId(IDS_STRING_BUILD_DATE);
		InnerGetStringInfo(hMod, dwGroupId);
		// 目前这两个字符串在一个Group里，所以Get一次就可以了
		dwGroupId = ResId2GroupId(IDS_STRING_MAX_DAYS);
		InnerGetStringInfo(hMod, dwGroupId);
		FreeLibrary(hMod);
	}
	else
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"为获取字符串信息加载%s失败", lpszExePath);
		return FALSE;
	}

	m_hExeFile = BeginUpdateResourceW(lpszExePath, FALSE);
	if (NULL==m_hExeFile)
	{
		WCHAR szDesc[MAX_PATH + 20] = { 0 };
		StringCbPrintfW(szDesc, sizeof(szDesc), L"打开文件%s以更新资源，失败", lpszExePath);
		return FALSE;
	}
	// 保存当前打开的文件路径
	StringCbCopyW(m_szCurExePath, sizeof(m_szCurExePath), lpszExePath);
	return TRUE;
}

BOOL CPEResInfo::Close(BOOL bSaveChange)
{
	BOOL bRet = TRUE;
	if (m_hExeFile)
	{
		// 先把缓存的字符串和版本资源都提交
		InnerUpdateResString();
		InnerUpdateVerInfo();

		// 把整个PE的修改提交
		BOOL bUpdateRet = EndUpdateResourceW(m_hExeFile, !bSaveChange);
		if (bUpdateRet)
		{
			if (bSaveChange)
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"已保存对%s资源的改动，关闭文件成功", m_szCurExePath);				
			}
			else
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"没有保存对%s资源的改动，关闭文件成功", m_szCurExePath);
			}
		}
		else
		{
			if (bSaveChange)
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"保存对%s资源的改动失败", m_szCurExePath);
			}
			else
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"没有保存对%s资源的改动，关闭文件失败", m_szCurExePath);
				
			}
			bRet = FALSE;
		}
		m_hExeFile = NULL;
	}
	ZeroMemory(m_szCurExePath, sizeof(m_szCurExePath));
	//放掉缓存
	if (m_verInfo.pData)
	{
		free(m_verInfo.pData);
		m_verInfo.pData = NULL;
	}
	ZeroMemory(&m_verInfo, sizeof(UPDATE_VERSION_RES_INFO));
	if (m_mapStringInfo.size()>0)
	{
		std::map<DWORD, STRING_RES>::iterator itStrInfo;
		for (itStrInfo=m_mapStringInfo.begin(); itStrInfo!=m_mapStringInfo.end(); ++itStrInfo)
		{
			if (itStrInfo->second.pData)
			{
				free(itStrInfo->second.pData);
				itStrInfo->second.pData = NULL;
			}
		}
		m_mapStringInfo.clear();
	}
	return bRet;
}

BOOL CPEResInfo::InnerGetVerInfo(LPCWSTR lpszExePath)
{
	DWORD dwVerSize = GetFileVersionInfoSizeW(lpszExePath, NULL);
	if (0==dwVerSize)
	{
		return FALSE;
	}
	LPBYTE pBuf = (LPBYTE)malloc(dwVerSize*sizeof(BYTE));
	if (NULL==pBuf)
	{
		return FALSE;
	}
	ZeroMemory(pBuf, dwVerSize*sizeof(BYTE));//这一行很关键
	if (FALSE==GetFileVersionInfoW(lpszExePath, 0, dwVerSize, pBuf))
	{
		free(pBuf);
		return FALSE;
	}
	m_verInfo.pData = pBuf;
	m_verInfo.cbDataSize = dwVerSize;
	m_verInfo.bModified = FALSE;
	// 拿语言信息
	LPLanguage lpTranslate = NULL;
	UINT unInfoLen = 0;
	if (VerQueryValueW(m_verInfo.pData, L"\\VarFileInfo\\Translation", (LPVOID *)&lpTranslate, &unInfoLen))
	{
		m_verInfo.wLang = lpTranslate->wLanguage;
		m_verInfo.wPage = lpTranslate->wCodePage;
		// 字符串的语言，也跟版本一样
		m_wLangID = lpTranslate->wLanguage;
	}
	else
	{
		m_verInfo.wLang = 2052; //默认是简体中文
		m_verInfo.wPage = 1200;
	}
	return TRUE;
}

BOOL CPEResInfo::InnerGetStringInfo(HMODULE hMod, DWORD dwGroupId)
{
	BOOL bRet = FALSE;
	// 先看看这个Group是不是已经缓存过了
	std::map<DWORD, STRING_RES>::iterator itFind;
	itFind = m_mapStringInfo.find(dwGroupId);
	if (m_mapStringInfo.end()!=itFind)
	{
		// 已经缓存过，无需再来一次
		return TRUE;
	}

	HRSRC hBlock = NULL;
	HGLOBAL hResData = NULL;
	LPVOID pData = NULL;
	DWORD dwResSize = 0;
	hBlock = FindResourceExW(hMod, RT_STRING, MAKEINTRESOURCEW(dwGroupId), m_wLangID);
	if (hBlock)
	{
		LPBYTE pBuf = NULL;
		dwResSize = SizeofResource(hMod, hBlock);
		// 至少要是32个字节才是正常的size，具体原因可见UpdateResString函数的注释
		if (dwResSize>31) 
		{
			pBuf = (LPBYTE)malloc(dwResSize);
		}
		hResData = LoadResource(hMod, hBlock);
		if (hResData)
		{
			pData = LockResource(hResData);
		}
		if (pData&&pBuf)
		{
			memcpy(pBuf, pData, dwResSize);
			STRING_RES sr;
			sr.dwGroupId = dwGroupId;
			sr.bModified = FALSE;
			sr.cbDataSize = dwResSize;
			sr.pData = pBuf;
			m_mapStringInfo.insert(std::make_pair(dwGroupId, sr));
			bRet = TRUE;
		}
		else
		{
			WCHAR szLogDesc[64] = { 0 };
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"定位Group为%u的字符串信息失败", dwGroupId);	
		}
	}
	else
	{
		WCHAR szLogDesc[64] = { 0 };
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"找不到Group为%u的字符串信息", dwGroupId);	
	}
	return bRet;
}

void CPEResInfo::SendLog(DWORD dwType, LPCWSTR lpDesc)
{
}

DWORD CPEResInfo::ResId2GroupId(DWORD dwResId)
{
	return dwResId/16 + 1;
}

BOOL CPEResInfo::UpdateResRCData(DWORD dwId, LPCWSTR lpszDataPath)
{
	BOOL bRet = FALSE;
	if (NULL==m_hExeFile)
	{
		return FALSE;
	}
	// 先把文件读入内存
	WCHAR szLogDesc[MAX_PATH + 30] = { 0 };
	LPBYTE pData = NULL;
	DWORD cbSize = 0, dwHasDone = 0;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	hFile = CreateFileW(lpszDataPath
		, GENERIC_READ
		, FILE_SHARE_READ | FILE_SHARE_READ | FILE_SHARE_DELETE
		, NULL
		, OPEN_EXISTING
		, 0
		, NULL
		);
	if (INVALID_HANDLE_VALUE==hFile)
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"尝试打开原始资源%s失败", lpszDataPath);
		return FALSE;
	}
	cbSize = GetFileSize(hFile, NULL);
	pData = (LPBYTE)malloc(cbSize);
	if (NULL==pData)
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"读取原始资源%s分配内存失败", lpszDataPath);
		CloseHandle(hFile);
		return FALSE;
	}
	if (FALSE==ReadFile(hFile, pData, cbSize, &dwHasDone, NULL) || dwHasDone!=cbSize)
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"读取原始资源文件%s失败", lpszDataPath);
		CloseHandle(hFile);
		return FALSE;
	}
	CloseHandle(hFile);
	// 数据已经准备好，可以合到资源里去了
	bRet = UpdateResourceW(m_hExeFile
		, RT_RCDATA
		, MAKEINTRESOURCEW(dwId)
		, m_wLangID
		, pData
		, cbSize
		);
	free(pData);
	return bRet;
}

BOOL CPEResInfo::UpdateResString(DWORD dwId, LPCWSTR lpNewValue, DWORD cchLen)
{
	// 字符串资源RT_STRING由一个一个的Group组成，每个Group有16个String，字符用Unicode编码存储
	// 每个String由字符串长度(用2个字节存储长度) + 字符串本身(每个字符2个字节)构成
	// 例如字符串“123”，其存储方式为
	//   00 03    00 31 00 32 00 33
	// <长度>   <---字符串本身--->
	// 如果所有字符串都为空，那么一个Group至少应该有2*16字节，全部都是0

	BOOL bRet = FALSE;
	if (0==cchLen)
	{
		cchLen = wcslen(lpNewValue);
		if (0==cchLen)
		{
			return FALSE;
		}
	}
	WCHAR szLogDesc[MAX_PATH + 20] = { 0 };
	DWORD dwGroup = ResId2GroupId(dwId);
	std::map<DWORD, STRING_RES>::iterator itFind;
	itFind = m_mapStringInfo.find(dwGroup);
	// 没有这项信息，创建一个新的Group
	if (m_mapStringInfo.end() == itFind)
	{
		// 空的Group，至少需要32个0
		DWORD cbNewSize = 32 + cchLen*2; 
		LPBYTE pBuf = (LPBYTE)malloc(cbNewSize);
		if (pBuf)
		{
			ZeroMemory(pBuf, cbNewSize);
			// 定位到dwId对应的字符串
			LPBYTE pBegin = pBuf + 2*(dwId%16);
			// 头两个字节写入字符串长度
			memcpy(pBegin, &cchLen, 2);
			pBegin += 2;
			// 接下来写字符串本身
			memcpy(pBegin, lpNewValue, cchLen*2);
			STRING_RES sr;
			sr.cbDataSize = cbNewSize;
			sr.bModified = TRUE;
			sr.dwGroupId = dwGroup;
			sr.pData = pBuf;
			m_mapStringInfo.insert(std::make_pair(dwGroup, sr));
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"已经创建了新的字符串Group %u", dwGroup);
			bRet = TRUE;
		}
		else
		{
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"创建新字符串Group %u时分配内存失败", dwGroup);
		}
	}
	else
	{
		// 更新已有的，这个要复杂许多了

		LPBYTE pBegin = itFind->second.pData;
		DWORD cbOrigDataSize = itFind->second.cbDataSize;

		DWORD i, chCurLen=0;
		// ID是dwId的字符串起始偏移
		DWORD dwPreOffset=0;
		// 字符串结尾的偏移(实际是下一个字符串的头)
		DWORD dwEndOffset=0;

		// 先定位到我们要修改的字符串上
		for (i=0; i<dwId - (dwGroup-1)*16; i++)
		{
			chCurLen = 0;
			// 定位到长度字段
			memcpy(&chCurLen, pBegin+dwPreOffset, 2);
			// 向后移动相应的长度，到下一个字符串
			dwPreOffset += chCurLen*2 + 2;
		}
		// 拿到当前字符串长度
		memcpy(&chCurLen, pBegin+dwPreOffset, 2);
		DWORD chOrigLen = chCurLen; //要修改的字符串的原始长度
		dwEndOffset = dwPreOffset + chCurLen*2 + 2; //顺便定位到下一个字符串开始

		// 计算新的总长度
		DWORD cbNewSize = cbOrigDataSize + cchLen*2 - chOrigLen*2;
		// 重新分配存储区
		LPBYTE pBuf = (LPBYTE)malloc(cbNewSize);
		if (pBuf)
		{
			LPBYTE pPos = pBuf; // 当前写入的起点
			// 要修改字符串之前的部分，原样拷过来
			if (dwPreOffset>0)
			{
				memcpy(pPos, pBegin, dwPreOffset);
			}
			pPos += dwPreOffset;
			// 写入新的字符串长度
			memcpy(pPos, &cchLen, 2);
			pPos += 2;
			// 写入字符串本身
			memcpy(pPos, lpNewValue, cchLen*2);
			pPos += cchLen*2;
			// 把修改字符串后面的部分也原样补到后面
			if (dwEndOffset<cbOrigDataSize)
			{
				memcpy(pPos, pBegin+dwEndOffset, cbOrigDataSize-dwEndOffset);
			}
			// 更新相应的项
			free(itFind->second.pData);
			itFind->second.pData = pBuf;
			itFind->second.bModified = TRUE;
			itFind->second.cbDataSize = cbNewSize;
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"已经成功更新了字符串Group %u", dwGroup);
			bRet = TRUE;
		}
		else
		{
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"更新字符串Group %u时分配内存失败", dwGroup);
		}
	}
	return bRet;
}

BOOL CPEResInfo::UpdateResVersion(DWORD* arrFileVer, DWORD* arrProdVer)
{
	if (NULL==m_verInfo.pData)
	{
		return FALSE;
	}
	WCHAR szLogDesc[64] = { 0 };
	VS_FIXEDFILEINFO* pFixInfo = NULL;
	UINT unInfoLen = 0;
	//先拿到固定结构中的版本信息，这些信息以数字方式存储
	if (VerQueryValueW(m_verInfo.pData, L"\\", (void**)&pFixInfo, &unInfoLen))
	{
		DWORD dwFVMS,dwFVLS;
		//更新版本信息
		if (arrFileVer)
		{
			dwFVMS =	((arrFileVer[0]&0xFFFF)<<16) | (arrFileVer[1]&0xFFFF);
			dwFVLS =		((arrFileVer[2]&0xFFFF)<<16) | (arrFileVer[3]&0xFFFF);
			pFixInfo->dwFileVersionMS= dwFVMS;
			pFixInfo->dwFileVersionLS = dwFVLS;
			m_bVerInfoModified = TRUE;
		}
		if (arrProdVer)
		{
			dwFVMS =	((arrProdVer[0]&0xFFFF)<<16) | (arrProdVer[1]&0xFFFF);
			dwFVLS =		((arrProdVer[2]&0xFFFF)<<16) | (arrProdVer[3]&0xFFFF);			
			pFixInfo->dwProductVersionMS = dwFVMS;
			pFixInfo->dwProductVersionLS = dwFVLS;
			m_bVerInfoModified = TRUE;
		}
	}
	else
	{
	}
	// 再看看可变部分的版本信息

	WCHAR szQueryCode[128] = { 0 };
	WCHAR szVerString[30] = { 0 };
	WCHAR* pOrigVer = NULL;
	if (arrFileVer)
	{
		// 修改可变部分的文件版本字符串
		StringCbPrintfW(szVerString, sizeof(szVerString)
			, L"%u\x2c\x20%u\x2c\x20%u\x2c\x20%u"
			, arrFileVer[0]
			, arrFileVer[1]
			, arrFileVer[2]
			, arrFileVer[3]
			);
		StringCbPrintfW(szQueryCode, sizeof(szQueryCode)
			, L"\\StringFileInfo\\%04x%04x\\FileVersion"
			, m_verInfo.wLang
			, m_verInfo.wPage
			);
		if (VerQueryValueW(m_verInfo.pData, szQueryCode, (LPVOID*)&pOrigVer, &unInfoLen))
		{
			wcsncpy(pOrigVer, szVerString, unInfoLen);
			if (wcslen(szVerString)>unInfoLen)
			{
				printf("文件版本可能会截断，此时需要重新更改版本号再编译文件\n");
			}
			else
			{
				StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"\"文件版本已修改为%s\"", pOrigVer);
				_tprintf(_T("%s\n"), szLogDesc);
				
				m_bVerInfoModified = TRUE;
			}
		}
	}

	if (arrProdVer)
	{
		// 产品版本字符串
		StringCbPrintfW(szVerString, sizeof(szVerString)
			, L"%u\x2c\x20%u\x2c\x20%u\x2c\x20%u"
			, arrProdVer[0]
			, arrProdVer[1]
			, arrProdVer[2]
			, arrProdVer[3]
		);
		StringCbPrintfW(szQueryCode, sizeof(szQueryCode)
			, L"\\StringFileInfo\\%04x%04x\\ProductVersion"
			, m_verInfo.wLang
			, m_verInfo.wPage
			);
		if (VerQueryValueW(m_verInfo.pData, szQueryCode, (LPVOID*)&pOrigVer, &unInfoLen))
		{
			wcsncpy(pOrigVer, szVerString, unInfoLen);
			if (wcslen(szVerString)>unInfoLen)
			{
				printf("文件版本可能会截断，此时需要重新更改版本号再编译文件\n");
			}
			else
			{
				StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"\"产品版本已修改为%s\"", pOrigVer);
				_tprintf(_T("%s\n"), szLogDesc);
				m_bVerInfoModified = TRUE;
			}
		}
	}
	return m_bVerInfoModified;
}

BOOL CPEResInfo::InnerUpdateResString()
{
	BOOL bRet = TRUE;
	if (m_hExeFile&&m_mapStringInfo.size()>0)
	{
		WCHAR szLogDesc[64] = { 0 };
		std::map<DWORD, STRING_RES>::iterator itStrRes;
		for (itStrRes=m_mapStringInfo.begin(); itStrRes!=m_mapStringInfo.end(); ++itStrRes)
		{
			if (itStrRes->second.bModified)
			{
				BOOL bUpdateRet = UpdateResourceW(m_hExeFile
					, RT_STRING
					, MAKEINTRESOURCEW(itStrRes->first)
					, m_wLangID
					, itStrRes->second.pData
					, itStrRes->second.cbDataSize
					);
				if (bUpdateRet)
				{
					StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"更新字符串Group %u成功", itStrRes->first);
				}
				else
				{
					StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"更新字符串Group %u失败", itStrRes->first);
					bRet = FALSE;
				}
			}
		}
	}
	return bRet;
}

BOOL CPEResInfo::InnerUpdateVerInfo()
{
	BOOL bRet = FALSE;
	if (FALSE==m_bVerInfoModified)
	{
		return TRUE;
	}
	if (NULL==m_hExeFile)
	{
		return FALSE;
	}
	if (NULL==m_verInfo.pData || 0==m_verInfo.cbDataSize)
	{
		return FALSE;
	}
	bRet = UpdateResourceW(m_hExeFile
		, RT_VERSION
		, MAKEINTRESOURCEW(VS_VERSION_INFO)
		, m_verInfo.wLang
		, m_verInfo.pData
		, m_verInfo.cbDataSize
		);
	if (bRet)
	{
	}
	else
	{
	}
	return bRet;
}

BOOL CPEResInfo::SetInstallerType(DWORD dwTypeFlag)
{
	BOOL bRet = FALSE;
	if (NULL==m_verInfo.pData)
	{
		return FALSE;
	}
	UINT unInfoLen = 0;
	WCHAR szQueryCode[128] = { 0 };
	WCHAR szLogDesc[64] = { 0 };
	WCHAR szFlag[128] = { 0 };
	LPWSTR pSpecial = NULL;
	StringCbPrintfW(szQueryCode, sizeof(szQueryCode)
		, L"StringFileInfo\\%04x%04x\\SpecialBuild"
		, m_verInfo.wLang
		, m_verInfo.wPage
		);
	if (VerQueryValue(m_verInfo.pData, szQueryCode, (LPVOID *)&pSpecial, &unInfoLen))
	{
		StringCbPrintfW(szFlag, sizeof(szFlag), L"%u", dwTypeFlag);
		DWORD dwNewLen = wcslen(szFlag);
		wcsncpy(pSpecial, szFlag, unInfoLen);
		// 字符串比原来的短，用0补齐
		if (dwNewLen<unInfoLen)
		{
			ZeroMemory(pSpecial+dwNewLen, 2*(unInfoLen-dwNewLen));
		}

		if (dwNewLen>=unInfoLen)
		{
		}
		else
		{
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"已经修改SpecialBuild字段为%s", szFlag);
		}
		bRet = TRUE;
	}
	else
	{
	}
	return bRet;
}