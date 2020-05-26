#include "QuickMem.h"

DWORD WINAPI GlobeQuickMemTimerThread(LPVOID v_lpParam)
{
	CQuickMem* pQuickMem = (CQuickMem*)v_lpParam;
	pQuickMem->Timer();

	return 0;
}

//////////////////////////////////////////////////////////////////////////
///	@brief Ĭ�Ϲ���
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

///	@brief ���촫��ָ����С
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

///	@brief �����������߳�ֹͣ���ڴ�����ͷ�
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

///	@brief �����ڴ�
///	@return ����ָ��m_iNodeSize��С�ڴ��ַ
PVOID CQuickMem::AllocateMem()
{
	PVOID			pBuf = NULL;
	PQUICKMEMNODE	pQuickMemNode = NULL;

	m_Lock.Lock();

	// ���Ȳ��Ҷ��У����Ƿ��п��нڵ㡣
	// ���û��FreeMem����û�п��нڵ��
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
			m_pMemList = m_pMemList->m_pNext; //��ǰ�ڴ��ʶ���趨Ϊ��һ��
		}

		pQuickMemNode->m_pNext = NULL;
		memset(pQuickMemNode->m_pBuf, 0, m_iNodeSize);
		pBuf = pQuickMemNode->m_pBuf;
	}
	else
	{
		if(m_iNodeSize > 0)
		{
			//ע����ʱpQuickMemNode�õ�malloc����һ�������ڴ��׵�ַ����pQuickMemNode->m_pBuf�ĵ�ֵַ�ر��׵�ַ�ߡ�
			//����Free����pQuickMemNode->m_pBufʱ��������ü��ķ�ʽ����λ���׵�ַ��Ȼ�����pQuickMemNode
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

///	@brief �ͷ��ڴ棬��߲������Free���ǰ��ڴ���Ϊ����
void CQuickMem::FreeMem(PVOID v_pBuf)
{
	m_Lock.Lock();

	//v_pBuf��ַ��Ըߣ���Ҫ���ü��ķ�ʽ��λ��pQuickMemNode�ĵ�ַ
	PQUICKMEMNODE pQuickMemNode = (PQUICKMEMNODE)((char*)v_pBuf - (sizeof(_tagQuickMemNode*) + sizeof(WORD)));
	pQuickMemNode->m_wTimeLeft = c_iQuickMemAliveTime;
	pQuickMemNode->m_pNext = NULL;

	//���нڵ㸳ֵ
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

///	@brief �������ڼ���ڴ�ڵ�����ʱ���̡߳�Ŀ�ģ�������ͷų�ʱ��δ��ʹ�õ��ڴ�ڵ㡣
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

///	@brief ���ڵ���������
void CQuickMem::DoTimer()
{
	m_Lock.Lock();

	// ����Ƿ��г�ʱû��ʹ�õ��ڴ�ڵ㣬����У����ͷ�
	PQUICKMEMNODE pQuickMemNode = m_pMemList;
	PQUICKMEMNODE pPrevNode = m_pMemList;
	PQUICKMEMNODE pDelNode = NULL;
	while(NULL != pQuickMemNode)
	{
		if(pQuickMemNode->m_wTimeLeft > 0)
		{
			pQuickMemNode->m_wTimeLeft--;

			pPrevNode = pQuickMemNode;
			pQuickMemNode = pQuickMemNode->m_pNext; //ָ����һ��
		}
		else
		{
			pDelNode = pQuickMemNode;

			if(m_pMemList == pQuickMemNode ) //�ͷ�ͷ�ڵ�
			{
				if(m_pTailNode == pQuickMemNode )
				{//�����ѿ�
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
			else if(m_pTailNode == pQuickMemNode) //�ͷ�β�ڵ�
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
