// IOCPServer.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <string>
#include <winsock2.h>   
#include <Windows.h>  
#include <vector>
#include <iostream>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")      // Socket编程需用的动态链接库   
//#pragma comment(lib, "Kernel32.lib") 
#define DefaultIP  "127.0.0.1"
#define DefaultPort 9999
#define DefaultClientNum 6000
#define MessMaxLen   1024
#define DataBuffSize   2 * 1024 

/**
* 结构体名称：PER_IO_DATA
* 结构体功能：重叠I/O需要用到的结构体，临时记录IO数据
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
* 结构体名称：PER_HANDLE_DATA
* 结构体存储：记录单个套接字的数据，包括了套接字的变量及套接字的对应的客户端的地址。
* 结构体作用：当服务器连接上客户端时，信息存储到该结构体中，知道客户端的地址以便于回访。
**/
typedef struct
{
	SOCKET socket;
	SOCKADDR_STORAGE ClientAddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

//单例类
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
	//私有方法
	CMYIOCPServer(void);
	bool  LoadWindowsSocket();
	bool InitServerSocket();
	bool CreateServerSocker();
	static void HandleMessage();
	//私有数据
	string m_sServerIP;
	int m_iLisenPoint;
	string m_sError;
	int m_iMaxClientNum;
	vector< PER_HANDLE_DATA* > m_vclientGroup;//保持客户端的连接信息  
	static HANDLE m_hMutex;//多线程访问互斥变量 
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
获得单例对象
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
类的构造函数
**************************/
CMYIOCPServer::CMYIOCPServer(void)
{
	m_iLisenPoint = DefaultPort;
}
/**************************
类的析构函数
**************************/
CMYIOCPServer::~CMYIOCPServer(void)
{
}
/**************************
设置服务器IP
**************************/
void CMYIOCPServer::SetServerIp(const string & sIP)
{
	m_sServerIP = sIP;
}

/**************************
设置服务器端口
**************************/
void  CMYIOCPServer::SetPort(const int &iPort)
{
	m_iLisenPoint = iPort;
}

/**************************
设置最大的客户端连接数目
**************************/
void  CMYIOCPServer::SetMaxClientNum(const int &iMaxNum)
{
	m_iMaxClientNum = iMaxNum;
}

/**************************
服务器接收客户端消息，
工作线程
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
			//这里不能返回，返回子线程就结束了
			//return -1;  
		}
		PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);

		// 检查在套接字上是否有错误发生   
		if (0 == BytesTransferred) {
			closesocket(PerHandleData->socket);
			GlobalFree(PerHandleData);
			GlobalFree(PerIoData);
			continue;
		}

		//得到消息码流
		memset(m_byteMsg, 0, MessMaxLen);
		memcpy(m_byteMsg, PerIoData->databuff.buf, MessMaxLen);
		//得到客户端SOCKET信息
		SOCKET sClientSocket = PerHandleData->socket;
		printf("message is %s \n", m_byteMsg);
		HandleMessage();
		//SendMessage(sClientSocket,m_byteMsg);
		// 为下一个重叠调用建立单I/O操作数据   
		ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED)); // 清空内存   
		PerIoData->databuff.len = 1024;
		PerIoData->databuff.buf = PerIoData->buffer;
		PerIoData->operationType = 0;    // read   
		WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);
	}
	return  0;
}

/*******************
处理消息类
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
	//将任务交给线程池处理
	//if (NULL != pCworkItem)
	//{
	//	m_CWorkQueue.InsertWorkItem(pCworkItem);
	//}

	printf("Finish Handle Message\n");
}

/**************************

发送消息给制定客户端
**************************/
void CMYIOCPServer::SendMessage(SOCKET &tSOCKET, char MessAge[MessMaxLen])
{
	// 开始数据处理，接收来自客户端的数据   
	WaitForSingleObject(m_hMutex, INFINITE);
	send(tSOCKET, MessAge, MessMaxLen, 0);  // 发送信息    
	ReleaseMutex(m_hMutex);
}

/**************************
初始化SOCKET对象，创建端口
和线程数组
**************************/
bool  CMYIOCPServer::LoadWindowsSocket()
{
	// 加载socket动态链接库   
	WORD wVersionRequested = MAKEWORD(2, 2); // 请求2.2版本的WinSock库   
	WSADATA wsaData;    // 接收Windows Socket的结构信息   
	DWORD err = WSAStartup(wVersionRequested, &wsaData);

	if (0 != err) {  // 检查套接字库是否申请成功   
		m_sError = "Request Windows Socket Library Error!\n";
		return false;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {// 检查是否申请了所需版本的套接字库   
		WSACleanup();
		m_sError = "Request Windows Socket Version 2.2 Error!\n";
		system("pause");
		return false;
	}

	// 创建IOCP的内核对象   
	/**
	* 需要用到的函数的原型：
	* HANDLE WINAPI CreateIoCompletionPort(
	*    __in   HANDLE FileHandle,     // 已经打开的文件句柄或者空句柄，一般是客户端的句柄
	*    __in   HANDLE ExistingCompletionPort, // 已经存在的IOCP句柄
	*    __in   ULONG_PTR CompletionKey,   // 完成键，包含了指定I/O完成包的指定文件
	*    __in   DWORD NumberOfConcurrentThreads // 真正并发同时执行最大线程数，一般推介是CPU核心数*2
	* );
	**/
	m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == m_completionPort) {    // 创建IO内核对象失败   
		m_sError = "CreateIoCompletionPort failed. Error:\n";
		return false;
	}

	// 创建IOCP线程--线程里面创建线程池    
	// 确定处理器的核心数量   
	SYSTEM_INFO mySysInfo;
	GetSystemInfo(&mySysInfo);

	// 基于处理器的核心数量创建线程   
	for (DWORD i = 0; i < (mySysInfo.dwNumberOfProcessors * 2); ++i) {
		// 创建服务器工作器线程，并将完成端口传递到该线程   
		HANDLE ThreadHandle = CreateThread(NULL, 0, &CMYIOCPServer::ServerWorkThread, m_completionPort, 0, NULL);
		if (NULL == ThreadHandle) {
			m_sError = "Create Thread Handle failed. Error::\n";
		}
		CloseHandle(ThreadHandle);
	}
	return true;
}
/*************************
初始化服务器SOCKET信息
*************************/
bool CMYIOCPServer::InitServerSocket()
{
	// 建立流式套接字   
	m_srvSocket = socket(AF_INET, SOCK_STREAM, 0);

	// 绑定SOCKET到本机   
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
创建服务器端的监听信息
**************************/
bool CMYIOCPServer::CreateServerSocker()
{

	// 将SOCKET设置为监听模式   
	int listenResult = listen(m_srvSocket, 10);
	if (SOCKET_ERROR == listenResult) {
		m_sError = "Listen failed. Error: ";
		return false;
	}

	// 开始处理IO数据   
	cout << "本服务器已准备就绪，正在等待客户端的接入...\n";
	int icount = 0;
	while (true) {
		PER_HANDLE_DATA * PerHandleData = NULL;
		SOCKADDR_IN saRemote;
		int RemoteLen;
		SOCKET acceptSocket;

		// 接收连接，并分配完成端，这儿可以用AcceptEx()   
		RemoteLen = sizeof(saRemote);
		acceptSocket = accept(m_srvSocket, (SOCKADDR*)&saRemote, &RemoteLen);
		if (SOCKET_ERROR == acceptSocket) {   // 接收客户端失败   
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

		// 创建用来和套接字关联的单句柄数据信息结构   
		PerHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));  // 在堆中为这个PerHandleData申请指定大小的内存   
		PerHandleData->socket = acceptSocket;
		memcpy(&PerHandleData->ClientAddr, &saRemote, RemoteLen);
		m_vclientGroup.push_back(PerHandleData);       // 将单个客户端数据指针放到客户端组中   

													   // 将接受套接字和完成端口关联   
		CreateIoCompletionPort((HANDLE)(PerHandleData->socket), m_completionPort, (DWORD)PerHandleData, 0);


		// 开始在接受套接字上处理I/O使用重叠I/O机制   
		// 在新建的套接字上投递一个或多个异步   
		// WSARecv或WSASend请求，这些I/O请求完成后，工作者线程会为I/O请求提供服务       
		// 单I/O操作数据(I/O重叠)   
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
	//销毁资源 
	DWORD dwByteTrans;
	PostQueuedCompletionStatus(m_completionPort, dwByteTrans, 0, 0);
	closesocket(listenResult);
	return true;
}

/*********************
启动服务器
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

