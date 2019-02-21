#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "resource.h"
#include <string>
#define BUFSIZE 512
#define ID_LISTBOX 100
#define ID_CHATBOX 101

using std::vector;

vector <RECT> rtList; // 도형을 저장해서 서버에서 관리하기 위해 저장.
vector <SOCKET> sockVec; // 소켓을 각각저장해서 도형이나 채팅 문자열을 보내주기 위해서 자료관리.
vector <char*> messageList; //채팅을 저장한것.

typedef struct dataList{
	RECT rt;
	char buf[BUFSIZE];
}dataList;

HANDLE hMutex; 

HWND hMainWnd;
HINSTANCE g_hinst;
dataList dl;
SOCKET listen_sock;

HWND hLog; // 로그창 
HWND hChat;  // 채팅창

char buf[BUFSIZE]; // 임시버퍼
char message[BUFSIZE]; // 메세지버퍼

void SyncPushSocket(const SOCKET client_sock)
	//클라이언트가 들어올때 마다 클라이언트의 통신 소켓값 을 저장
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
	//도형이 이미 존재할때, 만약 클라이언트가 새로 생성되면.
{
	
	const RECT rt ={0};
	int retval = 0;
	WaitForSingleObject(hMutex,INFINITE);
	//도형
	for(unsigned int i = 0; i< rtList.size(); ++i)
	{
		dataList dl;
		dl.rt = rtList[i];
		strcpy(dl.buf,"");
		//서버에 이미 그린 도형이 있는데, 그걸 클라이언트에게 보내줘야하니까
		///send로 보내줌.ㅣ
		retval = send(client_sock, (const char*)&dl, sizeof(dl), 0); 
		/// 새로 접속한 클라이언트 도형 동기화 

		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
	//채팅
	for(unsigned int i = 0; i <messageList.size(); ++i)
	{
		dataList dl;
		dl.rt = rt;
		strcpy(dl.buf,messageList[i]); //배열이기때문에 strcpy
		retval = send(client_sock, (const char*)&dl, sizeof(dl),0);

		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
	ReleaseMutex(hMutex);
}

void SyncPushRect(const RECT& rt)
	//도형이 클라에서 그려질때마다 저장.
{
	WaitForSingleObject(hMutex,INFINITE);
	rtList.push_back(rt);
	ReleaseMutex(hMutex);
}

void SyncSendData(const RECT& rt, const char * message)
{
	WaitForSingleObject(hMutex,INFINITE);
	int retval = 0; 
	wsprintf(buf,TEXT("[TCP 서버] 현재 접속중인 모든 클라이언트에 데이터를 전송합니다.\n"));
	SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);

	for(unsigned int i = 0; i< sockVec.size(); ++i)
	{
		SOCKET currentSocket = sockVec[i];// 모든 클라이언트 도형 동기화
		dl.rt = rt;
		retval = send(currentSocket, (char*)&dl, sizeof(dl), 0); 
		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
	ReleaseMutex(hMutex);
}

void SyncDeleteSocket(SOCKET client_sock)
{
	//마지막에 클라이언트가 종료되면 저장된 소켓도 지워야해서 사용
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
	static int count = 0;
	// 클라이언트 정보 얻기
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (SOCKADDR *)&clientaddr, &addrlen);

	SyncInitRect(client_sock);

	while(1){
		// 데이터 받기
		retval = recvn(client_sock, (char*)&dl, sizeof(dl), 0);
		if(retval == SOCKET_ERROR){
			err_display(TEXT("recv()"));
			break;
		}
		else if(retval == 0){
			break;
		}

		

		rt = dl.rt; // 수신된 도형을 저장.
		
		SyncPushRect(rt); // 미리 선언한 백터 rect 에 저장.
		
		InvalidateRect(hMainWnd,&rt,true);

		if(strlen(dl.buf) > 0) // 채팅이 유효한 문자가 들어오면 
		{
			
			WaitForSingleObject(hMutex,INFINITE);
			
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)dl.buf);
			messageList.push_back(dl.buf);
			ReleaseMutex(hMutex);
		}


		// 받은 데이터 출력

		wsprintf(buf,TEXT("[TCP/%s:%d] %d,%d,%d,%d\n"), inet_ntoa(clientaddr.sin_addr),
			ntohs(clientaddr.sin_port), rt.left,rt.top,rt.right,rt.bottom);
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		//로그 데이터 출력

		// 데이터 보내기
		SyncSendData(rt,dl.buf);
	}

	closesocket(client_sock);

	SyncDeleteSocket(client_sock);

	wsprintf(buf,"[TCP 서버] 클라이언트 도형 스레드 종료: IP 주소=%s, 포트 번호=%d, 현재 접속중인 클라이언트 : %d\n", 
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
	HANDLE hRectThread;
	unsigned ThreadId;

	while(1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);

		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			continue;
		}

		SyncPushSocket(client_sock); //connect된 클라이언트의 소켓 번호를 백터에 저장.
		wsprintf(buf,"[TCP 서버] 클라이언트 도형 스레드 접속: IP 주소=%s, 포트 번호=%d, 현재 접속한 클라이언트 수 : %d\n", 
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), sockVec.size());
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		// 스레드 생성
		hRectThread = (HANDLE)_beginthreadex(NULL, 0, ProcessClient, (LPVOID)client_sock, 0, &ThreadId);

		if(hRectThread == NULL)
		{
			wsprintf(buf,"[오류] 도형 스레드 생성 실패!\n");
			SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		}
		else
			CloseHandle(hRectThread);
	}
}

void InitRectSocket(short port)
{
	int retval;
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");   
	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
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
	static bool bLogShow = false;  //로그창 ON OFF
	static bool bChatShow = false; //채팅창 ON OFF
	switch (iMsg) 
	{
	case WM_CREATE:
		GetClientRect(hwnd,&rtClient);
		hMainWnd = hwnd;
		//초기화
		hLog=CreateWindow("listbox",NULL,WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL |
			LBS_NOTIFY,0,rtClient.bottom/2,rtClient.right,rtClient.bottom,hwnd,(HMENU)ID_LISTBOX,g_hinst,NULL);
		
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)"[TCP 서버] 로그 초기화 완료");
	
		if(!hLog == NULL)
			bLogShow = true;
		
		hChat=CreateWindow("listbox",NULL, WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY, 0, 0, rtClient.right/6, rtClient.bottom/2,hwnd,(HMENU)ID_CHATBOX,g_hinst,NULL);
		
		if(!(hChat == NULL))
			bChatShow = true;
		
		if(bChatShow)
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)"채팅 초기화 완료");
		else
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)"채팅 초기화 실패");

		InitRectSocket(9000); //Rect
		_beginthread(RecvThread,0,0); /// 소켓이 생성되고 커넥트가 된 후에, 샌드와 리시브가 가능하기 떄문에. 연결 후에 작성한다.
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)"로그 초기화 완료");
		break; 
	case WM_COMMAND: 
		//메뉴
		switch(LOWORD(wParam))
		{
		case ID_VIEW_LOG:
			if(bLogShow)
			{
				ShowWindow(hLog,SW_HIDE); //
				bLogShow = false;
			}
			else
			{
				ShowWindow(hLog,SW_SHOW);
				bLogShow = true;
			}
			break;
		case ID_VIEW_CHAT:
			if(bChatShow)
			{
				ShowWindow(hChat,SW_HIDE); //채팅창 숨기고
				bChatShow = false;
			}
			else
			{
				ShowWindow(hChat,SW_SHOW); //채팅창 보여주기
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