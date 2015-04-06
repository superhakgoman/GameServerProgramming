#include "stdafx.h"
#include "IocpManager.h"
#include "EduServer_IOCP.h"
#include "ClientSession.h"
#include "SessionManager.h"
#include "Exception.h"

#define GQCS_TIMEOUT	20

__declspec(thread) int LIoThreadId = 0;
IocpManager* GIocpManager = nullptr;

IocpManager::IocpManager() : mCompletionPort(NULL), mIoThreadCount(2), mListenSocket(NULL)
{
}


IocpManager::~IocpManager()
{
}

bool IocpManager::Initialize()
{
	//DONE: mIoThreadCount = ...;GetSystemInfo사용해서 set num of I/O threads
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	mIoThreadCount = sysInfo.dwNumberOfProcessors * 2;

	/// winsock initializing
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	/// Create I/O Completion Port
	//DONE: mCompletionPort = CreateIoCompletionPort(...)
	
	mCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!mCompletionPort)
	{
		printf_s("%d\n", GetLastError());
		return false;
	}

	/// create TCP socket
	//DONE: mListenSocket = ...
	
	mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (mListenSocket == INVALID_SOCKET)
	{
		printf_s("%d\n", WSAGetLastError());
		return false;
	}

	int opt = 1;
	setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

		//DONE:  bind

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(LISTEN_PORT);
	serverAddr.sin_addr.s_addr = htons(INADDR_ANY);
	
	int result = bind(mListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	
	if (result == SOCKET_ERROR)
	{
		printf_s("Socket error!\n");
		return false;
	}

	return true;
}


bool IocpManager::StartIoThreads()
{
	/// I/O Thread
	for (int i = 0; i < mIoThreadCount; ++i)
	{
		DWORD dwThreadId;
		//DONE: HANDLE hThread = (HANDLE)_beginthreadex...);

		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 
			0, 
			IoWorkerThread, 
			(LPVOID)i, 
			0, 
			(unsigned int*)&dwThreadId);

		if (hThread == 0)
		{
			printf_s("%d\n", GetLastError());
			return false;
		}
	}
	
	return true;
}


bool IocpManager::StartAcceptLoop()
{
	/// listen
	if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN))
		return false;


	/// accept loop
	while (true)
	{
		SOCKET acceptedSock = accept(mListenSocket, NULL, NULL);
		if (acceptedSock == INVALID_SOCKET)
		{
			printf_s("accept: invalid socket\n");
			continue;
		}

		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(acceptedSock, (SOCKADDR*)&clientaddr, &addrlen);

		/// 소켓 정보 구조체 할당과 초기화
		ClientSession* client = GSessionManager->CreateClientSession(acceptedSock);

		/// 클라 접속 처리
		if (false == client->OnConnect(&clientaddr))
		{
			client->Disconnect(DR_ONCONNECT_ERROR);
			GSessionManager->DeleteClientSession(client);
		}
	}

	return true;
}

void IocpManager::Finalize()
{
	CloseHandle(mCompletionPort);

	/// winsock finalizing
	WSACleanup();

}


unsigned int WINAPI IocpManager::IoWorkerThread(LPVOID lpParam)
{
	LThreadType = THREAD_IO_WORKER;

	LIoThreadId = reinterpret_cast<int>(lpParam);
	HANDLE hComletionPort = GIocpManager->GetComletionPort();

	while (true)
	{
		DWORD dwTransferred = 0;
		OverlappedIOContext* context = nullptr;
		ClientSession* asCompletionKey = nullptr;

		///DONE : <여기에는 GetQueuedCompletionStatus(hComletionPort, ..., GQCS_TIMEOUT)를 수행한 결과값을 대입
		int ret = GetQueuedCompletionStatus(hComletionPort, 
			&dwTransferred, 
			(PULONG_PTR)&asCompletionKey, 
			(LPOVERLAPPED*)&context, 
			GQCS_TIMEOUT);

		/// check time out first 
		if (ret == 0 && GetLastError()==WAIT_TIMEOUT)
			continue;

		if (ret == 0 || dwTransferred == 0)
		{
			/// connection closing
			asCompletionKey->Disconnect(DR_RECV_ZERO);
			GSessionManager->DeleteClientSession(asCompletionKey);
			continue;
		}

		// DONE : if (nullptr == context) 인 경우 처리
		//{
		//}

		if (context == nullptr)
		{
			CRASH_ASSERT(false);
			printf_s("OverLappedIOContext doesn't exists\n"); //iocp 객체 없음 : 서버 죽음 (타임아웃은 위에 GetLastError()로 처리)
			return 0;
		}

		bool completionOk = true;
		switch (context->mIoType)
		{
		case IO_SEND:
			completionOk = SendCompletion(asCompletionKey, context, dwTransferred);
			break;

		case IO_RECV:
			completionOk = ReceiveCompletion(asCompletionKey, context, dwTransferred);
			break;

		default:
			printf_s("Unknown I/O Type: %d\n", context->mIoType);
			break;
		}

		if ( !completionOk )
		{
			/// connection closing
			asCompletionKey->Disconnect(DR_COMPLETION_ERROR);
			GSessionManager->DeleteClientSession(asCompletionKey);
		}

	}

	return 0;
}

bool IocpManager::ReceiveCompletion(const ClientSession* client, OverlappedIOContext* context, DWORD dwTransferred)
{

	/// DONE : echo back 처리 client->PostSend()사용.
	if (client->PostSend(context->mBuffer, dwTransferred) == false)
	{
		delete context;
		printf_s("PostSend Error\n");
		return false;
	}
	
	delete context;

	return client->PostRecv();
}

bool IocpManager::SendCompletion(const ClientSession* client, OverlappedIOContext* context, DWORD dwTransferred)
{
	/// 전송 다 되었는지 확인하는 것 처리..
	//if (context->mWsaBuf.len != dwTransferred) {...}

	if (context->mWsaBuf.len != dwTransferred)
	{
		printf_s("SendCompletion error : buf length & transferred length are different.\n");
		delete context;
		return false;
	}
	
	delete context;
	return true;
}
