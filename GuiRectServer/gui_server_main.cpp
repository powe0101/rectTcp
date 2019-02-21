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

vector <RECT> rtList; // ������ �����ؼ� �������� �����ϱ� ���� ����.
vector <SOCKET> sockVec; // ������ ���������ؼ� �����̳� ä�� ���ڿ��� �����ֱ� ���ؼ� �ڷ����.
vector <char*> messageList; //ä���� �����Ѱ�.

typedef struct dataList{
	RECT rt;
	char buf[BUFSIZE];
}dataList;

HANDLE hMutex; 

HWND hMainWnd;
HINSTANCE g_hinst;
dataList dl;
SOCKET listen_sock;

HWND hLog; // �α�â 
HWND hChat;  // ä��â

char buf[BUFSIZE]; // �ӽù���
char message[BUFSIZE]; // �޼�������

void SyncPushSocket(const SOCKET client_sock)
	//Ŭ���̾�Ʈ�� ���ö� ���� Ŭ���̾�Ʈ�� ��� ���ϰ� �� ����
{
	WaitForSingleObject(hMutex,INFINITE);
	sockVec.push_back(client_sock);
	ReleaseMutex(hMutex);
}

// ���� �Լ� ���� ��� �� ����
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

// ���� �Լ� ���� ���
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
// Ŭ���̾�Ʈ�� ������ ���

void SyncInitRect(const SOCKET client_sock)
	//������ �̹� �����Ҷ�, ���� Ŭ���̾�Ʈ�� ���� �����Ǹ�.
{
	
	const RECT rt ={0};
	int retval = 0;
	WaitForSingleObject(hMutex,INFINITE);
	//����
	for(unsigned int i = 0; i< rtList.size(); ++i)
	{
		dataList dl;
		dl.rt = rtList[i];
		strcpy(dl.buf,"");
		//������ �̹� �׸� ������ �ִµ�, �װ� Ŭ���̾�Ʈ���� ��������ϴϱ�
		///send�� ������.��
		retval = send(client_sock, (const char*)&dl, sizeof(dl), 0); 
		/// ���� ������ Ŭ���̾�Ʈ ���� ����ȭ 

		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
	//ä��
	for(unsigned int i = 0; i <messageList.size(); ++i)
	{
		dataList dl;
		dl.rt = rt;
		strcpy(dl.buf,messageList[i]); //�迭�̱⶧���� strcpy
		retval = send(client_sock, (const char*)&dl, sizeof(dl),0);

		if(retval == SOCKET_ERROR){
			err_display(TEXT("send()"));
			break;
		}
	}
	ReleaseMutex(hMutex);
}

void SyncPushRect(const RECT& rt)
	//������ Ŭ�󿡼� �׷��������� ����.
{
	WaitForSingleObject(hMutex,INFINITE);
	rtList.push_back(rt);
	ReleaseMutex(hMutex);
}

void SyncSendData(const RECT& rt, const char * message)
{
	WaitForSingleObject(hMutex,INFINITE);
	int retval = 0; 
	wsprintf(buf,TEXT("[TCP ����] ���� �������� ��� Ŭ���̾�Ʈ�� �����͸� �����մϴ�.\n"));
	SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);

	for(unsigned int i = 0; i< sockVec.size(); ++i)
	{
		SOCKET currentSocket = sockVec[i];// ��� Ŭ���̾�Ʈ ���� ����ȭ
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
	//�������� Ŭ���̾�Ʈ�� ����Ǹ� ����� ���ϵ� �������ؼ� ���
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
	// Ŭ���̾�Ʈ ���� ���
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (SOCKADDR *)&clientaddr, &addrlen);

	SyncInitRect(client_sock);

	while(1){
		// ������ �ޱ�
		retval = recvn(client_sock, (char*)&dl, sizeof(dl), 0);
		if(retval == SOCKET_ERROR){
			err_display(TEXT("recv()"));
			break;
		}
		else if(retval == 0){
			break;
		}

		

		rt = dl.rt; // ���ŵ� ������ ����.
		
		SyncPushRect(rt); // �̸� ������ ���� rect �� ����.
		
		InvalidateRect(hMainWnd,&rt,true);

		if(strlen(dl.buf) > 0) // ä���� ��ȿ�� ���ڰ� ������ 
		{
			
			WaitForSingleObject(hMutex,INFINITE);
			
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)dl.buf);
			messageList.push_back(dl.buf);
			ReleaseMutex(hMutex);
		}


		// ���� ������ ���

		wsprintf(buf,TEXT("[TCP/%s:%d] %d,%d,%d,%d\n"), inet_ntoa(clientaddr.sin_addr),
			ntohs(clientaddr.sin_port), rt.left,rt.top,rt.right,rt.bottom);
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		//�α� ������ ���

		// ������ ������
		SyncSendData(rt,dl.buf);
	}

	closesocket(client_sock);

	SyncDeleteSocket(client_sock);

	wsprintf(buf,"[TCP ����] Ŭ���̾�Ʈ ���� ������ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d, ���� �������� Ŭ���̾�Ʈ : %d\n", 
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
		"���� GUI",
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

		SyncPushSocket(client_sock); //connect�� Ŭ���̾�Ʈ�� ���� ��ȣ�� ���Ϳ� ����.
		wsprintf(buf,"[TCP ����] Ŭ���̾�Ʈ ���� ������ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d, ���� ������ Ŭ���̾�Ʈ �� : %d\n", 
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), sockVec.size());
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)buf);
		// ������ ����
		hRectThread = (HANDLE)_beginthreadex(NULL, 0, ProcessClient, (LPVOID)client_sock, 0, &ThreadId);

		if(hRectThread == NULL)
		{
			wsprintf(buf,"[����] ���� ������ ���� ����!\n");
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
	static bool bLogShow = false;  //�α�â ON OFF
	static bool bChatShow = false; //ä��â ON OFF
	switch (iMsg) 
	{
	case WM_CREATE:
		GetClientRect(hwnd,&rtClient);
		hMainWnd = hwnd;
		//�ʱ�ȭ
		hLog=CreateWindow("listbox",NULL,WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL |
			LBS_NOTIFY,0,rtClient.bottom/2,rtClient.right,rtClient.bottom,hwnd,(HMENU)ID_LISTBOX,g_hinst,NULL);
		
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)"[TCP ����] �α� �ʱ�ȭ �Ϸ�");
	
		if(!hLog == NULL)
			bLogShow = true;
		
		hChat=CreateWindow("listbox",NULL, WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY, 0, 0, rtClient.right/6, rtClient.bottom/2,hwnd,(HMENU)ID_CHATBOX,g_hinst,NULL);
		
		if(!(hChat == NULL))
			bChatShow = true;
		
		if(bChatShow)
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)"ä�� �ʱ�ȭ �Ϸ�");
		else
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)"ä�� �ʱ�ȭ ����");

		InitRectSocket(9000); //Rect
		_beginthread(RecvThread,0,0); /// ������ �����ǰ� Ŀ��Ʈ�� �� �Ŀ�, ����� ���ú갡 �����ϱ� ������. ���� �Ŀ� �ۼ��Ѵ�.
		SendMessage(hLog,LB_ADDSTRING,0,(LPARAM)"�α� �ʱ�ȭ �Ϸ�");
		break; 
	case WM_COMMAND: 
		//�޴�
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
				ShowWindow(hChat,SW_HIDE); //ä��â �����
				bChatShow = false;
			}
			else
			{
				ShowWindow(hChat,SW_SHOW); //ä��â �����ֱ�
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