/** \file 
*	QuickMem.h 定义快速内存类
*	快速内存，其实就是内存复用技术。适用于频繁申请释放指定大小内存场景，达到快速的目的！
*
*	\author fzuim
*
*	版本历史
*	\par 2020-01-01
* 
*/

#pragma once
#include <Windows.h>

//节点生存周期
const int c_iQuickMemAliveTime = 10;

//快速内存节点，一字节对齐
#pragma pack(push,1)
typedef struct _tagQuickMemNode
{
	_tagQuickMemNode*	m_pNext;		//下一块内存指针
	WORD				m_wTimeLeft;	//内存生存周期
	char				m_pBuf[1];		//内存首地址
}QUICKMEMNODE, *PQUICKMEMNODE;
#pragma pack(pop)

class CCriticalSectionLock
{
public:
	CCriticalSectionLock(){
		InitializeCriticalSection(&m_cs);
	};

	~CCriticalSectionLock(){
		DeleteCriticalSection(&m_cs);
	}

	void Lock()
	{
		EnterCriticalSection(&m_cs);
	}

	void Unlock()
	{
		LeaveCriticalSection(&m_cs);
	}

private:
	CRITICAL_SECTION m_cs;
};

class CQuickMem
{
public:
	CQuickMem(void);
	CQuickMem(const int v_iNodeSize);
	~CQuickMem(void);

	PVOID AllocateMem();
	void FreeMem(PVOID v_pBuf);

protected:
	friend DWORD WINAPI GlobeQuickMemTimerThread(LPVOID v_lpParam);
	void InitTimerThread();
	void Timer();
	void DoTimer();

private:
	BOOL m_bStopTimerThd;

	int m_iNodeSize;

	DWORD m_dwAllocateTotal;
	DWORD m_dwFreeTotal;

	HANDLE m_hTimerThread;

	PQUICKMEMNODE m_pMemList;
	PQUICKMEMNODE m_pTailNode;
	CCriticalSectionLock m_Lock;
};