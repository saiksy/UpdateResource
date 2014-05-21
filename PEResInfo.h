// ����PE��Դ���޸ģ������ַ�����Դ���汾��Դ�Լ�RCDATA
#pragma once


#include <map>

// ������־����ʾ�ı�ģ�������
#define MODULE_NAME_PERESINFO  L"res"

//���尲װ��������
#define INSTALLER_TYPE_FLAG_NORMAL				0x00000000		//����ͨ�İ���Ҳ����mini��
#define INSTALLER_TYPE_FLAG_SILENT					0x00000001		//��һ����Ĭ��
#define INSTALLER_TYPE_FLAG_HIPS					0x00000002		//����hips
#define INSTALLER_TYPE_FLAG_AVIRA					0x00000004		//���к�ɡ������
#define INSTALLER_TYPE_FLAG_BITDEFENDER		0x00000008		//����BD������
#define INSTALLER_TYPE_FLAG_PROMOTION		0x00000010		//�ƹ���ʿ��������İ�
#define INSTALLER_TYPE_FLAG_DIFF						0x00000020		//�����ļ���װ��

//Ϊ�˼�鰲װ���Ƿ������ϰ汾 ��ʱ�������ֵ�ַ���
#define IDS_STRING_BUILD_DATE           601
#define IDS_STRING_MAX_DAYS             602

typedef struct _tagLanguage
{
	WORD wLanguage;
	WORD wCodePage;
} tagLanguage,  * LPLanguage;

typedef struct tagStringRes
{
	DWORD dwGroupId;
	BOOL bModified; //����ַ��������Ƿ��޸Ĺ�
	DWORD cbDataSize;
	LPBYTE	pData; 
}STRING_RES, LPSTRING_RES;

typedef struct tagUpdateVersionResInfo
{
	WORD wLang; //�汾��Ϣ����������
	WORD wPage;
	BOOL bModified;
	DWORD cbDataSize; //��������С
	LPBYTE pData; //���������ֽ�ָ��
}UPDATE_VERSION_RES_INFO;

class CPEResInfo
{
public:
	CPEResInfo();
	~CPEResInfo();
	BOOL Open(LPCWSTR lpszExePath); //��һ��exe�ļ�
	BOOL Close(BOOL bSaveChange);  //�ر�exe�ļ�������ָ���ǲ���Ҫ�����޸�

	//�޸��ַ�����Դ��Ϊ����֧��д�ض�����(���缴���ַ�����Ϊ4ҲҪд10���ַ�)�������Ҫ����cchLen��ָ��Ҫд����ַ�����
	//��Ȼ�������ָ��cchLenΪ0��������ʹ��\0��Ϊ�ַ�����β����д��ĳ���
	//�������ֻ�޸��ڴ���Ļ��棬�������ύ��PE�ļ�
	//Ҫ�������ַ����Ķ��ύ��PE�ļ�����Ҫ���ŵ���InnerUpdateResString
	BOOL UpdateResString(DWORD dwId, LPCWSTR lpNewValue, DWORD cchLen); 
	//�޸���Դ�İ汾��Ϣ��arrFileVer��arrProdVer��ָ���ļ��Ͳ�Ʒ�汾�����飬ָ��Ϊ�ձ�ʾ���޸�
	BOOL UpdateResVersion(DWORD* arrFileVer, DWORD* arrProdVer);
	//�޸�RCDATA�ڵ㣬Ҳ�������밲װ�ļ��ĸ���cab��
	BOOL UpdateResRCData(DWORD dwId, LPCWSTR lpszDataPath);

	//���ð�װ�������ͣ������Ǿ�Ĭ/��hips/��������/�ƹ�ȱ�־λ����ϣ����嶨�����ͷ�ļ�����
	//����ʵ���Ǹ�дVersion��Դ���SpecialBulid�ֶ�
	BOOL SetInstallerType(DWORD dwTypeFlag);

	static CString GetFileVersion( const CString& strFilePath, DWORD *pdwV1 /*= 0*/, DWORD *pdwV2 /*= 0*/, DWORD *pdwV3 /*= 0*/, DWORD *pdwV4 /*= 0*/ );
private:
	void SendLog(DWORD dwType, LPCWSTR lpDesc);
	// Ϊ�˾���������BeginUpdateResource�Ⱥ����Ķ������ڴ�һ��PE�޸�ǰ���Ȱ���Ҫ��������Ϣ���������
	// ���һ���԰������޸��ύ���
	BOOL InnerGetVerInfo(LPCWSTR lpszExePath); //����Version��Ϣ
	BOOL InnerGetStringInfo(HMODULE hMod, DWORD dwGroupId); //�����ַ�����Ϣ
	DWORD ResId2GroupId(DWORD dwResId);
	BOOL InnerUpdateResString();
	BOOL InnerUpdateVerInfo();
	
private:
	HANDLE m_hExeFile;
	WCHAR m_szCurExePath[MAX_PATH];
	WORD m_wLangID;//��װ�����ַ���ʹ�õ�����
	UPDATE_VERSION_RES_INFO m_verInfo; //����������VersionInfo�����Ϣ
	BOOL m_bVerInfoModified;
	std::map<DWORD, STRING_RES> m_mapStringInfo; // ����������StringInfo��ÿ��Groupռ��mapһ��item
};