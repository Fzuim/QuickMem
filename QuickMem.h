/** \file 
*	QuickMem.h ��������ڴ���
*	�����ڴ棬��ʵ�����ڴ渴�ü�����������Ƶ�������ͷ�ָ����С�ڴ泡�����ﵽ���ٵ�Ŀ�ģ�
*
*	\author fzuim
*
*	�汾��ʷ
*	\par 2020-01-01
* 
*/

#pragma once
#include <Windows.h>

//�ڵ���������
const int c_iQuickMemAliveTime = 10;

//�����ڴ�ڵ㣬һ�ֽڶ���
#pragma pack(push,1)
typedef struct _tagQuickMemNode
{
	_tagQuickMemNode*	m_pNext;		//��һ���ڴ�ָ��
	WORD				m_wTimeLeft;	//�ڴ���������
	char				m_pBuf[1];		//�ڴ��׵�ַ
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