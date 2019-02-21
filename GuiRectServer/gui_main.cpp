#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "resource.h"

#define BUFSIZE 512
#define ID_LISTBOX 100
#define ID_CHATBOX 101
#define ID_SEND_BUTTON 102
using std::vector;

vector <RECT> rtList;
vector <SOCKET> sockVec;

HANDLE hMutex;
HWND hMainWnd;
HINSTANCE g_hinst;

SOCKET listen_sock;

HWND hLog;
HWND hChat;
HWND hSend;
char buf[BUFSIZE] = "[TCP 서버] 초기화 완료";

void SyncPushSocket(const SOCKET client_sock)
{
	WaitForSingleObject(hMutex,INFINITE);
	sockVec.push_back(client_sock);
	ReleaseMutex(hMutex);
}

// 소켓 함수 오류 출력 후 종료
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER|
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(-1);
}

// 소켓 함수 오류 출력
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER|
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	
	wsprintf(buf,"[%s] %s", msg, (LPCTSTR)lpMsgBuf);
	SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
	InvalidateRect(hMainWnd,NULL,TRUE);
	LocalFree(lpMsgBuf);
}

int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0){
		received = recv(s, ptr, left, flags);
		if(received == SOCKET_ERROR) 
			return SOCKET_ERROR;
		else if(received == 0) 
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}
// 클라이언트와 데이터 통신

void SyncInitRect(const SOCKET client_sock)
{
	int retval = 0;
	WaitForSingleObject(hMutex,INFINITE);
	for(unsigned int i = 0; i< rtList.size(); ++i)
	{
		retval = send(client_sock, (const char*)&rtList[i], sizeof(rtList[i]), 0); 
		/// 새로 접속한 클라이언트 도형 동기화
		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
	ReleaseMutex(hMutex);
}

void SyncPushRect(const RECT& rt)
{
	WaitForSingleObject(hMutex,INFINITE);
	rtList.push_back(rt);
	ReleaseMutex(hMutex);
}

void SyncSendRect(const RECT& rt)
{
	int retval = 0; 
	wsprintf(buf,TEXT("[TCP 서버] 현재 접속중인 모든 클라이언트에 그린 도형을 전송합니다.\n"));
	SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
	for(unsigned int i = 0; i< sockVec.size(); ++i)
	{
		SOCKET currentSocket = sockVec[i];// 모든 클라이언트 도형 동기화

		retval = send(currentSocket, (char*)&rt, sizeof(RECT), 0); 
		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
}

void SyncDeleteSocket(SOCKET client_sock)
{
	unsigned int i;
	for(i = 0; i< sockVec.size(); ++i)
	{
		SOCKET currentSocket = sockVec[i];
		if(client_sock == currentSocket)
		{
			break;
		}
	}
	sockVec.erase(sockVec.begin()+i);
}

unsigned WINAPI ProcessClient(LPVOID arg)
{
	SOCKET client_sock = (SOCKET)arg;
	SOCKADDR_IN clientaddr;
	int addrlen;
	int retval;
	RECT rt;
	// 클라이언트 정보 얻기
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (SOCKADDR *)&clientaddr, &addrlen);

	SyncInitRect(client_sock);

	while(1){
		// 데이터 받기
		retval = recvn(client_sock, (char*)&rt, sizeof(RECT), 0);
		if(retval == SOCKET_ERROR){
			err_display(TEXT("recv()"));
			break;
		}
		else if(retval == 0){
			break;
		}

		SyncPushRect(rt);
		InvalidateRect(hMainWnd,NULL,true);
		// 받은 데이터 출력
		wsprintf(buf,TEXT("[TCP/%s:%d] %d,%d,%d,%d\n"), inet_ntoa(clientaddr.sin_addr),
			ntohs(clientaddr.sin_port), rt.left,rt.top,rt.right,rt.bottom);
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		// 데이터 보내기
		SyncSendRect(rt);

		// closesocket()
		
	}
	closesocket(client_sock);

	SyncDeleteSocket(client_sock);

	wsprintf(buf,"[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d, 현재 접속중인 클라이언트 : %d\n", 
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port),sockVec.size());
	SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
	return 0;

}
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return -1;
	hMutex = CreateMutex(NULL,FALSE,NULL);

	g_hinst  = hInstance;
	HWND 	 hwnd;
	MSG 	 msg;
	WNDCLASS WndClass;   
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = WndProc;		
	WndClass.cbClsExtra	= 0;		
	WndClass.cbWndExtra	= 0;		
	WndClass.hInstance = hInstance;		
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);	
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	
	WndClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);		
	WndClass.lpszClassName = "Window Class Name";	
	RegisterClass(&WndClass);	
	hwnd = CreateWindow("Window Class Name",
		"서버 GUI",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,	
		CW_USEDEFAULT,	
		CW_USEDEFAULT,	
		CW_USEDEFAULT,	
		NULL,	
		NULL,	
		hInstance,	
		NULL	 
	);

	
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);	
		DispatchMessage(&msg);	
	}   
	
	CloseHandle(hMutex);
	WSACleanup();
	
	return (int)msg.wParam;
}
void DrawRect(HDC hdc, RECT* prt)
{
	Rectangle(hdc,prt->left,prt->top,prt->right,prt->bottom);
}

void RecvThread(LPVOID pParam)
{
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	HANDLE hThread;
	unsigned ThreadId;
	while(1){
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);

		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			continue;
		}

		SyncPushSocket(client_sock);
		wsprintf(buf,"[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d, 현재 접속한 클라이언트 수 : %d\n", 
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), sockVec.size());
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		// 스레드 생성
		hThread = (HANDLE)_beginthreadex(NULL, 0, ProcessClient, 
			(LPVOID)client_sock, 0, &ThreadId);

		if(hThread == NULL)
		{
			wsprintf(buf,"[오류] 스레드 생성 실패!\n");
			SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		}
		else
			CloseHandle(hThread);
	}
}

void InitSocket()
{
	int retval;
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");   
	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(9000);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HDC			hdc;
	PAINTSTRUCT ps;
	static RECT rtClient;
	static bool bLogShow = true;
	static bool bChatShow = true;
	switch (iMsg) 
	{
	case WM_CREATE:
		GetClientRect(hwnd,&rtClient);
		hMainWnd = hwnd;
		hLog=CreateWindow("listbox",NULL,WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL |
			LBS_NOTIFY,0,rtClient.bottom/2,rtClient.right,rtClient.bottom,hwnd,(HMENU)ID_LISTBOX,g_hinst,NULL);
		hChat=CreateWindow("listbox",NULL, WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY, 0, 0, rtClient.right/6, rtClient.bottom/2,hwnd,(HMENU)ID_CHATBOX,g_hinst,NULL);
		hSend=CreateWindow("button",NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY, 0, rtClient.bottom/2+20, 50, 50, hwnd, (HMENU)ID_SEND_BUTTON,g_hinst,NULL);
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)"채팅 초기화 완료");
		InitSocket();
		_beginthread(RecvThread,0,0); /// 소켓이 생성되고 커넥트가 된 후에, 샌드와 리시브가 가능하기 떄문에. 연결 후에 작성한다.
		break; 
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case ID_VIEW_LOG:
			if(bLogShow)
			{
				ShowWindow(hLog,SW_HIDE);
				bLogShow = false;
			}
			else
			{
				ShowWindow(hLog,SW_SHOW);
				bLogShow = true;
			}
			break;
		case ID_VIEW_RECT:
			if(bChatShow)
			{
				ShowWindow(hChat,SW_HIDE);
				bChatShow = false;
			}
			else
			{
				ShowWindow(hChat,SW_SHOW);
				bChatShow = true;
			}
			break;
		}
		break;
	case WM_SIZE:
		GetClientRect(hwnd,&rtClient);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		for(unsigned int i = 0; i < rtList.size(); ++i)
			DrawRect(hdc,&rtList[i]);
		EndPaint(hwnd, &ps);
		break; 
	case WM_DESTROY:
		closesocket(listen_sock);
		PostQuitMessage(0);
		break; 
	}
	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}