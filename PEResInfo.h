// 负责PE资源的修改，包括字符串资源，版本资源以及RCDATA
#pragma once


#include <map>

// 用于日志中显示的本模块的名字
#define MODULE_NAME_PERESINFO  L"res"

//定义安装包的类型
#define INSTALLER_TYPE_FLAG_NORMAL				0x00000000		//最普通的包，也就是mini包
#define INSTALLER_TYPE_FLAG_SILENT					0x00000001		//是一个静默包
#define INSTALLER_TYPE_FLAG_HIPS					0x00000002		//带有hips
#define INSTALLER_TYPE_FLAG_AVIRA					0x00000004		//带有红伞病毒库
#define INSTALLER_TYPE_FLAG_BITDEFENDER		0x00000008		//带有BD病毒库
#define INSTALLER_TYPE_FLAG_PROMOTION		0x00000010		//推广卫士和浏览器的包
#define INSTALLER_TYPE_FLAG_DIFF						0x00000020		//差异文件安装包

//为了检查安装包是否已是老版本 的时间戳和阈值字符串
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
	BOOL bModified; //这个字符串数据是否被修改过
	DWORD cbDataSize;
	LPBYTE	pData; 
}STRING_RES, LPSTRING_RES;

typedef struct tagUpdateVersionResInfo
{
	WORD wLang; //版本信息所属的语言
	WORD wPage;
	BOOL bModified;
	DWORD cbDataSize; //数据区大小
	LPBYTE pData; //数据区首字节指针
}UPDATE_VERSION_RES_INFO;

class CPEResInfo
{
public:
	CPEResInfo();
	~CPEResInfo();
	BOOL Open(LPCWSTR lpszExePath); //打开一个exe文件
	BOOL Close(BOOL bSaveChange);  //关闭exe文件，可以指定是不是要保存修改

	//修改字符串资源。为了能支持写特定长度(例如即便字符串长为4也要写10个字符)，你必须要传入cchLen来指定要写入的字符数。
	//当然，你可以指定cchLen为0，函数将使用\0作为字符串结尾计算写入的长度
	//这个函数只修改内存里的缓存，不真正提交到PE文件
	//要把所有字符串改动提交给PE文件，需要接着调用InnerUpdateResString
	BOOL UpdateResString(DWORD dwId, LPCWSTR lpNewValue, DWORD cchLen); 
	//修改资源的版本信息，arrFileVer和arrProdVer是指定文件和产品版本的数组，指针为空表示不修改
	BOOL UpdateResVersion(DWORD* arrFileVer, DWORD* arrProdVer);
	//修改RCDATA节点，也就是捆入安装文件的各个cab包
	BOOL UpdateResRCData(DWORD dwId, LPCWSTR lpszDataPath);

	//设置安装包的类型，可以是静默/带hips/带病毒库/推广等标志位的组合，具体定义见本头文件顶部
	//具体实现是改写Version资源里的SpecialBulid字段
	BOOL SetInstallerType(DWORD dwTypeFlag);

	static CString GetFileVersion( const CString& strFilePath, DWORD *pdwV1 /*= 0*/, DWORD *pdwV2 /*= 0*/, DWORD *pdwV3 /*= 0*/, DWORD *pdwV4 /*= 0*/ );
private:
	void SendLog(DWORD dwType, LPCWSTR lpDesc);
	// 为了尽量不干扰BeginUpdateResource等函数的动作，在打开一个PE修改前，先把需要的其他信息都缓存出来
	// 最后一次性把所有修改提交完成
	BOOL InnerGetVerInfo(LPCWSTR lpszExePath); //缓存Version信息
	BOOL InnerGetStringInfo(HMODULE hMod, DWORD dwGroupId); //缓存字符串信息
	DWORD ResId2GroupId(DWORD dwResId);
	BOOL InnerUpdateResString();
	BOOL InnerUpdateVerInfo();
	
private:
	HANDLE m_hExeFile;
	WCHAR m_szCurExePath[MAX_PATH];
	WORD m_wLangID;//安装程序字符串使用的语言
	UPDATE_VERSION_RES_INFO m_verInfo; //缓存起来的VersionInfo相关信息
	BOOL m_bVerInfoModified;
	std::map<DWORD, STRING_RES> m_mapStringInfo; // 缓存起来的StringInfo，每个Group占用map一个item
};