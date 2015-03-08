#pragma comment(lib, "ws2_32.lib") //Winsock 사용을 위해 필요

#include "stdafx.h"
#include <WinSock2.h>	//Winsock 함수들 포함. winsock과 winsock2의 차이는 뭘까
#include <stdio.h>

#define BUFSIZE 4096	//버퍼 크기
#define PORTNUM 9999	//비둘기야 먹자
#define WM_SOCKET WM_USER +1 //WM_USER : private 메시지를 만드는 데 사용. WM_USER + n 형태로 사용. WM_APP과 정확히 어떻게 다른지 잘 모르겠다. 

SOCKET listenerSocket = NULL;
char* className = "EchoServer";

bool initListener(HWND hwnd){
	WSADATA wsaData;
	SOCKADDR_IN addr;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){
		printError("WSAStartUp", WSAGetLastError());
		return false;
	}

	listenerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenerSocket == INVALID_SOCKET){
		printError("socket", WSAGetLastError());
		return false;
	}
	
	memset((void*)&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORTNUM);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int result = bind(listenerSocket, (struct sockaddr*)&addr, sizeof(addr));
	if (result != 0){
		printError("bind", WSAGetLastError());
		return false;
	}

	if (listen(listenerSocket, SOMAXCONN) != 0)
	{
		printError("listen", WSAGetLastError());
		return false;
	}

	if (WSAAsyncSelect(listenerSocket, hwnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE) != 0){
		printError("WSAAsyncSelect", WSAGetLastError());
		return false;
	}

	return true;
}

void printError(const char* processName, int errCode){
	printf_s("There was an error while running \"%s\", Error code : %d\n", processName, errCode);
	return;
}

LRESULT CALLBACK Winproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

}

int _tmain(int argc, _TCHAR* argv[])
{
	MSG msg;
	WNDCLASSEX windowClass;
	HWND window;

	windowClass.cbSize = sizeof(WNDCLASSEX); //size of this structure. set before calling the GetClassInfoEx().
	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW; // class style. can be any combination of the Window Class Styles. 
	windowClass.lpfnWndProc = (WNDPROC)Winproc; // pointer to window procedure. (WindowProc callback function)
	windowClass.cbClsExtra = 0; // The number of extra bytes to allocate following the window-class structure. The system initializes the bytes to zero.
	windowClass.cbWndExtra = 0; // extra bytes to allocate following the window instance.
	windowClass.hInstance = NULL; //A handle to the instance that contains the window procedure for the class.
	windowClass.hIcon = NULL; //handle to the class icon. must be a handle to an icon resource. NULL == default icon.
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW); //handle to the class cursor. handle to a cursor resource.
	windowClass.hbrBackground = (HBRUSH)COLOR_WINDOWFRAME; // handle to the class background brush.
	windowClass.lpszMenuName = NULL; //Pointer to a null-terminated character string that specifies the resource name of the class menu. NULL == no default menu.
	windowClass.lpszClassName = (LPCWSTR)className; // specifies the window class name. The class name can be any name registered with RegisterClass or RegisterClassEx. max 256
	windowClass.hIconSm = NULL; //handle to a small icon.

	RegisterClassEx(&windowClass);

	return 0;
}

