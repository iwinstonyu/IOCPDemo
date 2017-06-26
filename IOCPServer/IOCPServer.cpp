// IOCPServer.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#include <string>
#include <winsock2.h>   
#include <Windows.h>  
#include <vector>
#include <iostream>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")      // Socket������õĶ�̬���ӿ�   
//#pragma comment(lib, "Kernel32.lib") 
#define DefaultIP  "127.0.0.1"
#define DefaultPort 9999
#define DefaultClientNum 6000
#define MessMaxLen   1024
#define DataBuffSize   2 * 1024 

/**
* �ṹ�����ƣ�PER_IO_DATA
* �ṹ�幦�ܣ��ص�I/O��Ҫ�õ��Ľṹ�壬��ʱ��¼IO����
**/
typedef struct
{
	OVERLAPPED overlapped;
	WSABUF databuff;
	char buffer[DataBuffSize];
	int BufferLen;
	int operationType;
	SOCKET socket;
}PER_IO_OPERATEION_DATA, *LPPER_IO_OPERATION_DATA, *LPPER_IO_DATA, PER_IO_DATA;

/**
* �ṹ�����ƣ�PER_HANDLE_DATA
* �ṹ��洢����¼�����׽��ֵ����ݣ��������׽��ֵı������׽��ֵĶ�Ӧ�Ŀͻ��˵ĵ�ַ��
* �ṹ�����ã��������������Ͽͻ���ʱ����Ϣ�洢���ýṹ���У�֪���ͻ��˵ĵ�ַ�Ա��ڻطá�
**/
typedef struct
{
	SOCKET socket;
	SOCKADDR_STORAGE ClientAddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

//������
class CMYIOCPServer
{
public:
	~CMYIOCPServer(void);
	bool ServerSetUp();
	void SetServerIp(const string & sIP = DefaultIP);
	void SetPort(const int &iPort = DefaultPort);
	void SetMaxClientNum(const int &iMaxNum = DefaultClientNum);
	static DWORD WINAPI  ServerWorkThread(LPVOID CompletionPortID);
	static void SendMessage(SOCKET &tSOCKET, char MessAge[MessMaxLen]);
	static CMYIOCPServer* GetInstance();
private:
	//˽�з���
	CMYIOCPServer(void);
	bool  LoadWindowsSocket();
	bool InitServerSocket();
	bool CreateServerSocker();
	static void HandleMessage();
	//˽������
	string m_sServerIP;
	int m_iLisenPoint;
	string m_sError;
	int m_iMaxClientNum;
	vector< PER_HANDLE_DATA* > m_vclientGroup;//���ֿͻ��˵�������Ϣ  
	static HANDLE m_hMutex;//���̷߳��ʻ������ 
	static HANDLE m_completionPort;
	SOCKET m_srvSocket;
	static CMYIOCPServer *m_pInstance;
	static char m_byteMsg[MessMaxLen];
};

HANDLE  CMYIOCPServer::m_completionPort = NULL;
HANDLE CMYIOCPServer::m_hMutex = NULL;
CMYIOCPServer* CMYIOCPServer::m_pInstance = NULL;
char CMYIOCPServer::m_byteMsg[MessMaxLen] = { 0 };
/**************************
��õ�������
***************************/
CMYIOCPServer* CMYIOCPServer::GetInstance()
{
	if (NULL == m_pInstance)
	{
		m_pInstance = new CMYIOCPServer();
	}
	return m_pInstance;
}
/**************************
��Ĺ��캯��
**************************/
CMYIOCPServer::CMYIOCPServer(void)
{
	m_iLisenPoint = DefaultPort;
}
/**************************
�����������
**************************/
CMYIOCPServer::~CMYIOCPServer(void)
{
}
/**************************
���÷�����IP
**************************/
void CMYIOCPServer::SetServerIp(const string & sIP)
{
	m_sServerIP = sIP;
}

/**************************
���÷������˿�
**************************/
void  CMYIOCPServer::SetPort(const int &iPort)
{
	m_iLisenPoint = iPort;
}

/**************************
�������Ŀͻ���������Ŀ
**************************/
void  CMYIOCPServer::SetMaxClientNum(const int &iMaxNum)
{
	m_iMaxClientNum = iMaxNum;
}

/**************************
���������տͻ�����Ϣ��
�����߳�
**************************/
DWORD WINAPI   CMYIOCPServer::ServerWorkThread(LPVOID CompletionPortID)
{
	HANDLE CompletionPort = (HANDLE)CompletionPortID;
	DWORD BytesTransferred;
	LPOVERLAPPED IpOverlapped;
	LPPER_HANDLE_DATA PerHandleData = NULL;
	LPPER_IO_DATA PerIoData = NULL;
	DWORD RecvBytes;
	DWORD Flags = 0;
	BOOL bRet = false;

	while (true) {
		bRet = GetQueuedCompletionStatus(m_completionPort, &BytesTransferred, (PULONG_PTR)&PerHandleData, (LPOVERLAPPED*)&IpOverlapped, INFINITE);
		if (bRet == 0) {
			cerr << "GetQueuedCompletionStatus Error: " << GetLastError() << endl;
			continue;
			//���ﲻ�ܷ��أ��������߳̾ͽ�����
			//return -1;  
		}
		PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);

		// ������׽������Ƿ��д�����   
		if (0 == BytesTransferred) {
			closesocket(PerHandleData->socket);
			GlobalFree(PerHandleData);
			GlobalFree(PerIoData);
			continue;
		}

		//�õ���Ϣ����
		memset(m_byteMsg, 0, MessMaxLen);
		memcpy(m_byteMsg, PerIoData->databuff.buf, MessMaxLen);
		//�õ��ͻ���SOCKET��Ϣ
		SOCKET sClientSocket = PerHandleData->socket;
		printf("message is %s \n", m_byteMsg);
		HandleMessage();
		//SendMessage(sClientSocket,m_byteMsg);
		// Ϊ��һ���ص����ý�����I/O��������   
		ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED)); // ����ڴ�   
		PerIoData->databuff.len = 1024;
		PerIoData->databuff.buf = PerIoData->buffer;
		PerIoData->operationType = 0;    // read   
		WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);
	}
	return  0;
}

/*******************
������Ϣ��
*******************/
void CMYIOCPServer::HandleMessage()
{
	printf("thread is %d\n", GetCurrentThreadId());
	//WorkItemBase *pCworkItem = NULL;
	byte iCommand = m_byteMsg[0];
	switch (iCommand)
	{
	case  '0':
		//pCworkItem = new CWorkA();
		break;
	case '1':
		//pCworkItem = new CWorkB();
		break;

	case '2':
		break;
	default:
		break;
	}
	//�����񽻸��̳߳ش���
	//if (NULL != pCworkItem)
	//{
	//	m_CWorkQueue.InsertWorkItem(pCworkItem);
	//}

	printf("Finish Handle Message\n");
}

/**************************

������Ϣ���ƶ��ͻ���
**************************/
void CMYIOCPServer::SendMessage(SOCKET &tSOCKET, char MessAge[MessMaxLen])
{
	// ��ʼ���ݴ����������Կͻ��˵�����   
	WaitForSingleObject(m_hMutex, INFINITE);
	send(tSOCKET, MessAge, MessMaxLen, 0);  // ������Ϣ    
	ReleaseMutex(m_hMutex);
}

/**************************
��ʼ��SOCKET���󣬴����˿�
���߳�����
**************************/
bool  CMYIOCPServer::LoadWindowsSocket()
{
	// ����socket��̬���ӿ�   
	WORD wVersionRequested = MAKEWORD(2, 2); // ����2.2�汾��WinSock��   
	WSADATA wsaData;    // ����Windows Socket�Ľṹ��Ϣ   
	DWORD err = WSAStartup(wVersionRequested, &wsaData);

	if (0 != err) {  // ����׽��ֿ��Ƿ�����ɹ�   
		m_sError = "Request Windows Socket Library Error!\n";
		return false;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {// ����Ƿ�����������汾���׽��ֿ�   
		WSACleanup();
		m_sError = "Request Windows Socket Version 2.2 Error!\n";
		system("pause");
		return false;
	}

	// ����IOCP���ں˶���   
	/**
	* ��Ҫ�õ��ĺ�����ԭ�ͣ�
	* HANDLE WINAPI CreateIoCompletionPort(
	*    __in   HANDLE FileHandle,     // �Ѿ��򿪵��ļ�������߿վ����һ���ǿͻ��˵ľ��
	*    __in   HANDLE ExistingCompletionPort, // �Ѿ����ڵ�IOCP���
	*    __in   ULONG_PTR CompletionKey,   // ��ɼ���������ָ��I/O��ɰ���ָ���ļ�
	*    __in   DWORD NumberOfConcurrentThreads // ��������ͬʱִ������߳�����һ���ƽ���CPU������*2
	* );
	**/
	m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == m_completionPort) {    // ����IO�ں˶���ʧ��   
		m_sError = "CreateIoCompletionPort failed. Error:\n";
		return false;
	}

	// ����IOCP�߳�--�߳����洴���̳߳�    
	// ȷ���������ĺ�������   
	SYSTEM_INFO mySysInfo;
	GetSystemInfo(&mySysInfo);

	// ���ڴ������ĺ������������߳�   
	for (DWORD i = 0; i < (mySysInfo.dwNumberOfProcessors * 2); ++i) {
		// �����������������̣߳�������ɶ˿ڴ��ݵ����߳�   
		HANDLE ThreadHandle = CreateThread(NULL, 0, &CMYIOCPServer::ServerWorkThread, m_completionPort, 0, NULL);
		if (NULL == ThreadHandle) {
			m_sError = "Create Thread Handle failed. Error::\n";
		}
		CloseHandle(ThreadHandle);
	}
	return true;
}
/*************************
��ʼ��������SOCKET��Ϣ
*************************/
bool CMYIOCPServer::InitServerSocket()
{
	// ������ʽ�׽���   
	m_srvSocket = socket(AF_INET, SOCK_STREAM, 0);

	// ��SOCKET������   
	SOCKADDR_IN srvAddr;
	srvAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(m_iLisenPoint);
	int bindResult = bind(m_srvSocket, (SOCKADDR*)&srvAddr, sizeof(SOCKADDR));
	if (SOCKET_ERROR == bindResult) {
		m_sError = "Bind failed. Error:";
		return false;
	}
	return true;
}
/**************************
�����������˵ļ�����Ϣ
**************************/
bool CMYIOCPServer::CreateServerSocker()
{

	// ��SOCKET����Ϊ����ģʽ   
	int listenResult = listen(m_srvSocket, 10);
	if (SOCKET_ERROR == listenResult) {
		m_sError = "Listen failed. Error: ";
		return false;
	}

	// ��ʼ����IO����   
	cout << "����������׼�����������ڵȴ��ͻ��˵Ľ���...\n";
	int icount = 0;
	while (true) {
		PER_HANDLE_DATA * PerHandleData = NULL;
		SOCKADDR_IN saRemote;
		int RemoteLen;
		SOCKET acceptSocket;

		// �������ӣ���������ɶˣ����������AcceptEx()   
		RemoteLen = sizeof(saRemote);
		acceptSocket = accept(m_srvSocket, (SOCKADDR*)&saRemote, &RemoteLen);
		if (SOCKET_ERROR == acceptSocket) {   // ���տͻ���ʧ��   
			cerr << "Accept Socket Error: " << GetLastError() << endl;
			m_sError = "Accept Socket Error: ";
			icount++;
			if (icount > 50)
			{
				return false;
			}
			continue;
		}
		icount = 0;

		// �����������׽��ֹ����ĵ����������Ϣ�ṹ   
		PerHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));  // �ڶ���Ϊ���PerHandleData����ָ����С���ڴ�   
		PerHandleData->socket = acceptSocket;
		memcpy(&PerHandleData->ClientAddr, &saRemote, RemoteLen);
		m_vclientGroup.push_back(PerHandleData);       // �������ͻ�������ָ��ŵ��ͻ�������   

													   // �������׽��ֺ���ɶ˿ڹ���   
		CreateIoCompletionPort((HANDLE)(PerHandleData->socket), m_completionPort, (DWORD)PerHandleData, 0);


		// ��ʼ�ڽ����׽����ϴ���I/Oʹ���ص�I/O����   
		// ���½����׽�����Ͷ��һ�������첽   
		// WSARecv��WSASend������ЩI/O������ɺ󣬹������̻߳�ΪI/O�����ṩ����       
		// ��I/O��������(I/O�ص�)   
		LPPER_IO_OPERATION_DATA PerIoData = NULL;
		PerIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATEION_DATA));
		ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED));
		PerIoData->databuff.len = 1024;
		PerIoData->databuff.buf = PerIoData->buffer;
		PerIoData->operationType = 0;    // read   

		DWORD RecvBytes;
		DWORD Flags = 0;
		WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);
	}
	//������Դ 
	DWORD dwByteTrans;
	PostQueuedCompletionStatus(m_completionPort, dwByteTrans, 0, 0);
	closesocket(listenResult);
	return true;
}

/*********************
����������
*********************/
bool CMYIOCPServer::ServerSetUp()
{
	if (false == LoadWindowsSocket())
	{
		return false;
	}
	if (false == InitServerSocket())
	{
		return false;
	}
	if (false == CreateServerSocker())
	{
		return false;
	}
	return true;
}


int main()
{
	CMYIOCPServer* pServer = CMYIOCPServer::GetInstance();
	pServer->ServerSetUp();
    return 0;
}

