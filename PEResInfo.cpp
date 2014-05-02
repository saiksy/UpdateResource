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
	// �ȰѰ汾��Ϣ�Ȼ��������Ȼ����BeginUpdateResource���ļ�
	if (FALSE==InnerGetVerInfo(lpszExePath))
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"��ȡ%s�İ汾��Դ��Ϣʧ��", lpszExePath);
	}

	// �ַ�������Ϣ
	HMODULE hMod = LoadLibraryW(lpszExePath);
	if (hMod)
	{
		// ����Ŀǰֻ�����������ַ�������Ϣ
		DWORD dwGroupId = ResId2GroupId(IDS_STRING_BUILD_DATE);
		InnerGetStringInfo(hMod, dwGroupId);
		// Ŀǰ�������ַ�����һ��Group�����Getһ�ξͿ�����
		dwGroupId = ResId2GroupId(IDS_STRING_MAX_DAYS);
		InnerGetStringInfo(hMod, dwGroupId);
		FreeLibrary(hMod);
	}
	else
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"Ϊ��ȡ�ַ�����Ϣ����%sʧ��", lpszExePath);
		return FALSE;
	}

	m_hExeFile = BeginUpdateResourceW(lpszExePath, FALSE);
	if (NULL==m_hExeFile)
	{
		WCHAR szDesc[MAX_PATH + 20] = { 0 };
		StringCbPrintfW(szDesc, sizeof(szDesc), L"���ļ�%s�Ը�����Դ��ʧ��", lpszExePath);
		return FALSE;
	}
	// ���浱ǰ�򿪵��ļ�·��
	StringCbCopyW(m_szCurExePath, sizeof(m_szCurExePath), lpszExePath);
	return TRUE;
}

BOOL CPEResInfo::Close(BOOL bSaveChange)
{
	BOOL bRet = TRUE;
	if (m_hExeFile)
	{
		// �Ȱѻ�����ַ����Ͱ汾��Դ���ύ
		InnerUpdateResString();
		InnerUpdateVerInfo();

		// ������PE���޸��ύ
		BOOL bUpdateRet = EndUpdateResourceW(m_hExeFile, !bSaveChange);
		if (bUpdateRet)
		{
			if (bSaveChange)
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"�ѱ����%s��Դ�ĸĶ����ر��ļ��ɹ�", m_szCurExePath);				
			}
			else
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"û�б����%s��Դ�ĸĶ����ر��ļ��ɹ�", m_szCurExePath);
			}
		}
		else
		{
			if (bSaveChange)
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"�����%s��Դ�ĸĶ�ʧ��", m_szCurExePath);
			}
			else
			{
				WCHAR szDesc[MAX_PATH + 20] = { 0 };
				StringCbPrintfW(szDesc, sizeof(szDesc), L"û�б����%s��Դ�ĸĶ����ر��ļ�ʧ��", m_szCurExePath);
				
			}
			bRet = FALSE;
		}
		m_hExeFile = NULL;
	}
	ZeroMemory(m_szCurExePath, sizeof(m_szCurExePath));
	//�ŵ�����
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
	ZeroMemory(pBuf, dwVerSize*sizeof(BYTE));//��һ�кܹؼ�
	if (FALSE==GetFileVersionInfoW(lpszExePath, 0, dwVerSize, pBuf))
	{
		free(pBuf);
		return FALSE;
	}
	m_verInfo.pData = pBuf;
	m_verInfo.cbDataSize = dwVerSize;
	m_verInfo.bModified = FALSE;
	// ��������Ϣ
	LPLanguage lpTranslate = NULL;
	UINT unInfoLen = 0;
	if (VerQueryValueW(m_verInfo.pData, L"\\VarFileInfo\\Translation", (LPVOID *)&lpTranslate, &unInfoLen))
	{
		m_verInfo.wLang = lpTranslate->wLanguage;
		m_verInfo.wPage = lpTranslate->wCodePage;
		// �ַ��������ԣ�Ҳ���汾һ��
		m_wLangID = lpTranslate->wLanguage;
	}
	else
	{
		m_verInfo.wLang = 2052; //Ĭ���Ǽ�������
		m_verInfo.wPage = 1200;
	}
	return TRUE;
}

BOOL CPEResInfo::InnerGetStringInfo(HMODULE hMod, DWORD dwGroupId)
{
	BOOL bRet = FALSE;
	// �ȿ������Group�ǲ����Ѿ��������
	std::map<DWORD, STRING_RES>::iterator itFind;
	itFind = m_mapStringInfo.find(dwGroupId);
	if (m_mapStringInfo.end()!=itFind)
	{
		// �Ѿ����������������һ��
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
		// ����Ҫ��32���ֽڲ���������size������ԭ��ɼ�UpdateResString������ע��
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
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"��λGroupΪ%u���ַ�����Ϣʧ��", dwGroupId);	
		}
	}
	else
	{
		WCHAR szLogDesc[64] = { 0 };
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�Ҳ���GroupΪ%u���ַ�����Ϣ", dwGroupId);	
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
	// �Ȱ��ļ������ڴ�
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
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"���Դ�ԭʼ��Դ%sʧ��", lpszDataPath);
		return FALSE;
	}
	cbSize = GetFileSize(hFile, NULL);
	pData = (LPBYTE)malloc(cbSize);
	if (NULL==pData)
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"��ȡԭʼ��Դ%s�����ڴ�ʧ��", lpszDataPath);
		CloseHandle(hFile);
		return FALSE;
	}
	if (FALSE==ReadFile(hFile, pData, cbSize, &dwHasDone, NULL) || dwHasDone!=cbSize)
	{
		StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"��ȡԭʼ��Դ�ļ�%sʧ��", lpszDataPath);
		CloseHandle(hFile);
		return FALSE;
	}
	CloseHandle(hFile);
	// �����Ѿ�׼���ã����Ժϵ���Դ��ȥ��
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
	// �ַ�����ԴRT_STRING��һ��һ����Group��ɣ�ÿ��Group��16��String���ַ���Unicode����洢
	// ÿ��String���ַ�������(��2���ֽڴ洢����) + �ַ�������(ÿ���ַ�2���ֽ�)����
	// �����ַ�����123������洢��ʽΪ
	//   00 03    00 31 00 32 00 33
	// <����>   <---�ַ�������--->
	// ��������ַ�����Ϊ�գ���ôһ��Group����Ӧ����2*16�ֽڣ�ȫ������0

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
	// û��������Ϣ������һ���µ�Group
	if (m_mapStringInfo.end() == itFind)
	{
		// �յ�Group��������Ҫ32��0
		DWORD cbNewSize = 32 + cchLen*2; 
		LPBYTE pBuf = (LPBYTE)malloc(cbNewSize);
		if (pBuf)
		{
			ZeroMemory(pBuf, cbNewSize);
			// ��λ��dwId��Ӧ���ַ���
			LPBYTE pBegin = pBuf + 2*(dwId%16);
			// ͷ�����ֽ�д���ַ�������
			memcpy(pBegin, &cchLen, 2);
			pBegin += 2;
			// ������д�ַ�������
			memcpy(pBegin, lpNewValue, cchLen*2);
			STRING_RES sr;
			sr.cbDataSize = cbNewSize;
			sr.bModified = TRUE;
			sr.dwGroupId = dwGroup;
			sr.pData = pBuf;
			m_mapStringInfo.insert(std::make_pair(dwGroup, sr));
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�Ѿ��������µ��ַ���Group %u", dwGroup);
			bRet = TRUE;
		}
		else
		{
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�������ַ���Group %uʱ�����ڴ�ʧ��", dwGroup);
		}
	}
	else
	{
		// �������еģ����Ҫ���������

		LPBYTE pBegin = itFind->second.pData;
		DWORD cbOrigDataSize = itFind->second.cbDataSize;

		DWORD i, chCurLen=0;
		// ID��dwId���ַ�����ʼƫ��
		DWORD dwPreOffset=0;
		// �ַ�����β��ƫ��(ʵ������һ���ַ�����ͷ)
		DWORD dwEndOffset=0;

		// �ȶ�λ������Ҫ�޸ĵ��ַ�����
		for (i=0; i<dwId - (dwGroup-1)*16; i++)
		{
			chCurLen = 0;
			// ��λ�������ֶ�
			memcpy(&chCurLen, pBegin+dwPreOffset, 2);
			// ����ƶ���Ӧ�ĳ��ȣ�����һ���ַ���
			dwPreOffset += chCurLen*2 + 2;
		}
		// �õ���ǰ�ַ�������
		memcpy(&chCurLen, pBegin+dwPreOffset, 2);
		DWORD chOrigLen = chCurLen; //Ҫ�޸ĵ��ַ�����ԭʼ����
		dwEndOffset = dwPreOffset + chCurLen*2 + 2; //˳�㶨λ����һ���ַ�����ʼ

		// �����µ��ܳ���
		DWORD cbNewSize = cbOrigDataSize + cchLen*2 - chOrigLen*2;
		// ���·���洢��
		LPBYTE pBuf = (LPBYTE)malloc(cbNewSize);
		if (pBuf)
		{
			LPBYTE pPos = pBuf; // ��ǰд������
			// Ҫ�޸��ַ���֮ǰ�Ĳ��֣�ԭ��������
			if (dwPreOffset>0)
			{
				memcpy(pPos, pBegin, dwPreOffset);
			}
			pPos += dwPreOffset;
			// д���µ��ַ�������
			memcpy(pPos, &cchLen, 2);
			pPos += 2;
			// д���ַ�������
			memcpy(pPos, lpNewValue, cchLen*2);
			pPos += cchLen*2;
			// ���޸��ַ�������Ĳ���Ҳԭ����������
			if (dwEndOffset<cbOrigDataSize)
			{
				memcpy(pPos, pBegin+dwEndOffset, cbOrigDataSize-dwEndOffset);
			}
			// ������Ӧ����
			free(itFind->second.pData);
			itFind->second.pData = pBuf;
			itFind->second.bModified = TRUE;
			itFind->second.cbDataSize = cbNewSize;
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�Ѿ��ɹ��������ַ���Group %u", dwGroup);
			bRet = TRUE;
		}
		else
		{
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�����ַ���Group %uʱ�����ڴ�ʧ��", dwGroup);
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
	//���õ��̶��ṹ�еİ汾��Ϣ����Щ��Ϣ�����ַ�ʽ�洢
	if (VerQueryValueW(m_verInfo.pData, L"\\", (void**)&pFixInfo, &unInfoLen))
	{
		DWORD dwFVMS,dwFVLS;
		//���°汾��Ϣ
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
	// �ٿ����ɱ䲿�ֵİ汾��Ϣ

	WCHAR szQueryCode[128] = { 0 };
	WCHAR szVerString[30] = { 0 };
	WCHAR* pOrigVer = NULL;
	if (arrFileVer)
	{
		// �޸Ŀɱ䲿�ֵ��ļ��汾�ַ���
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
				printf("�ļ��汾���ܻ�ضϣ���ʱ��Ҫ���¸��İ汾���ٱ����ļ�\n");
			}
			else
			{
				StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"\"�ļ��汾���޸�Ϊ%s\"", pOrigVer);
				_tprintf(_T("%s\n"), szLogDesc);
				
				m_bVerInfoModified = TRUE;
			}
		}
	}

	if (arrProdVer)
	{
		// ��Ʒ�汾�ַ���
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
				printf("�ļ��汾���ܻ�ضϣ���ʱ��Ҫ���¸��İ汾���ٱ����ļ�\n");
			}
			else
			{
				StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"\"��Ʒ�汾���޸�Ϊ%s\"", pOrigVer);
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
					StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�����ַ���Group %u�ɹ�", itStrRes->first);
				}
				else
				{
					StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�����ַ���Group %uʧ��", itStrRes->first);
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
		// �ַ�����ԭ���Ķ̣���0����
		if (dwNewLen<unInfoLen)
		{
			ZeroMemory(pSpecial+dwNewLen, 2*(unInfoLen-dwNewLen));
		}

		if (dwNewLen>=unInfoLen)
		{
		}
		else
		{
			StringCbPrintfW(szLogDesc, sizeof(szLogDesc), L"�Ѿ��޸�SpecialBuild�ֶ�Ϊ%s", szFlag);
		}
		bRet = TRUE;
	}
	else
	{
	}
	return bRet;
}