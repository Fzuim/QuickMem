#include "QuickMem.h"

DWORD WINAPI GlobeQuickMemTimerThread(LPVOID v_lpParam)
{
	CQuickMem* pQuickMem = (CQuickMem*)v_lpParam;
	pQuickMem->Timer();

	return 0;
}

//////////////////////////////////////////////////////////////////////////
///	@brief 默认构造
CQuickMem::CQuickMem(void)
{
	m_iNodeSize = 0;
	m_pMemList = NULL;
	m_pTailNode = NULL;
	m_hTimerThread = NULL;
	m_dwAllocateTotal = 0;
	m_dwFreeTotal = 0;

	m_bStopTimerThd = FALSE;

	InitTimerThread();
}

///	@brief 构造传入指定大小
CQuickMem::CQuickMem(const int v_iNodeSize)
{
	m_iNodeSize = v_iNodeSize;
	m_pMemList = NULL;
	m_pTailNode = NULL;
	m_hTimerThread = NULL;
	m_dwAllocateTotal = 0;
	m_dwFreeTotal = 0;

	m_bStopTimerThd = FALSE;

	InitTimerThread();
}

///	@brief 析构，进行线程停止和内存队列释放
CQuickMem::~CQuickMem(void)
{
	m_bStopTimerThd = TRUE;
	if(NULL != m_hTimerThread)
	{
		if (WAIT_OBJECT_0 != WaitForSingleObject(m_hTimerThread, 100L))
		{
			::TerminateThread(m_hTimerThread, 0);
		}

		CloseHandle(m_hTimerThread);
		m_hTimerThread = NULL;
	}
	
	while(NULL != m_pMemList)
	{
		PQUICKMEMNODE pQuickMemNode = m_pMemList;
		m_pMemList = m_pMemList->m_pNext;

		pQuickMemNode->m_pNext = NULL;
		free(pQuickMemNode);
	}

	m_pMemList = NULL;
	m_pTailNode = NULL;
}

///	@brief 分配内存
///	@return 返回指定m_iNodeSize大小内存地址
PVOID CQuickMem::AllocateMem()
{
	PVOID			pBuf = NULL;
	PQUICKMEMNODE	pQuickMemNode = NULL;

	m_Lock.Lock();

	// 首先查找队列，看是否有空闲节点。
	// 如果没有FreeMem，是没有空闲节点的
	if(NULL != m_pMemList)
	{
		pQuickMemNode = m_pMemList;

		if( m_pTailNode == pQuickMemNode )
		{
			m_pTailNode = NULL;
			m_pMemList = NULL;
		}
		else
		{
			m_pMemList = m_pMemList->m_pNext; //当前内存标识，设定为下一块
		}

		pQuickMemNode->m_pNext = NULL;
		memset(pQuickMemNode->m_pBuf, 0, m_iNodeSize);
		pBuf = pQuickMemNode->m_pBuf;
	}
	else
	{
		if(m_iNodeSize > 0)
		{
			//注：此时pQuickMemNode拿到malloc返回一块连续内存首地址，而pQuickMemNode->m_pBuf的地址值必比首地址高。
			//后续Free传入pQuickMemNode->m_pBuf时，必须采用减的方式，定位到首地址，然后操作pQuickMemNode
			pQuickMemNode = (PQUICKMEMNODE)malloc(m_iNodeSize + sizeof(_tagQuickMemNode*) + sizeof(WORD));
			pQuickMemNode->m_pNext = NULL;
			pQuickMemNode->m_wTimeLeft = c_iQuickMemAliveTime;
			memset(pQuickMemNode->m_pBuf, 0, m_iNodeSize);
			pBuf = pQuickMemNode->m_pBuf;

			m_dwAllocateTotal++;
		}
	}

	m_Lock.Unlock();

	return pBuf;
}

///	@brief 释放内存，这边并非真的Free而是把内存标记为空闲
void CQuickMem::FreeMem(PVOID v_pBuf)
{
	m_Lock.Lock();

	//v_pBuf地址相对高，需要采用减的方式定位到pQuickMemNode的地址
	PQUICKMEMNODE pQuickMemNode = (PQUICKMEMNODE)((char*)v_pBuf - (sizeof(_tagQuickMemNode*) + sizeof(WORD)));
	pQuickMemNode->m_wTimeLeft = c_iQuickMemAliveTime;
	pQuickMemNode->m_pNext = NULL;

	//空闲节点赋值
	if(NULL == m_pMemList)
	{
		m_pMemList = pQuickMemNode;
		m_pTailNode = pQuickMemNode;
	}
	else
	{
		m_pTailNode->m_pNext = pQuickMemNode;
		m_pTailNode = pQuickMemNode;
	}

	m_Lock.Unlock();
}

///	@brief 创建定期检查内存节点生存时间线程。目的：清除并释放长时间未被使用的内存节点。
void CQuickMem::InitTimerThread()
{
	if(NULL == m_hTimerThread)
	{
		DWORD dwThreadId = 0;
		m_hTimerThread = ::CreateThread(NULL, 0, GlobeQuickMemTimerThread, this, 0, &dwThreadId);
	}
}

void CQuickMem::Timer()
{
	while(!m_bStopTimerThd)
	{
		DoTimer();
		Sleep(1000L);
	}
}

///	@brief 检查节点生存周期
void CQuickMem::DoTimer()
{
	m_Lock.Lock();

	// 检查是否有超时没有使用的内存节点，如果有，则释放
	PQUICKMEMNODE pQuickMemNode = m_pMemList;
	PQUICKMEMNODE pPrevNode = m_pMemList;
	PQUICKMEMNODE pDelNode = NULL;
	while(NULL != pQuickMemNode)
	{
		if(pQuickMemNode->m_wTimeLeft > 0)
		{
			pQuickMemNode->m_wTimeLeft--;

			pPrevNode = pQuickMemNode;
			pQuickMemNode = pQuickMemNode->m_pNext; //指向下一个
		}
		else
		{
			pDelNode = pQuickMemNode;

			if(m_pMemList == pQuickMemNode ) //释放头节点
			{
				if(m_pTailNode == pQuickMemNode )
				{//队列已空
					m_pMemList = NULL;
					m_pTailNode = NULL;
					pPrevNode = NULL;
					pQuickMemNode = NULL;
				}
				else
				{
					m_pMemList = m_pMemList->m_pNext;
					pPrevNode = m_pMemList;
					pQuickMemNode = pPrevNode;
				}
			}
			else if(m_pTailNode == pQuickMemNode) //释放尾节点
			{
				pPrevNode->m_pNext = NULL;
				m_pTailNode = pPrevNode;
				pQuickMemNode = NULL;
			}
			else
			{
				pPrevNode->m_pNext = pQuickMemNode->m_pNext;
				pQuickMemNode = pQuickMemNode->m_pNext;
			}

			pDelNode->m_pNext = NULL;
			free(pDelNode);

			m_dwFreeTotal++;
		}
	}

	m_Lock.Unlock();
}
