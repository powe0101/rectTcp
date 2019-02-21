#include <WinSock2.h>
#include <process.h>
#include <windows.h>
#include <vector>

using std::vector;

#define BUFSIZE 512 //char buf[] �� ũ�⸦ ���ϱ� ���� ������ ��.
#define ID_CHATBOX 101 // 
#define ID_SEND_BUTTON 102
#define ID_MESSAGE_BOX 103


LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, 
						 WPARAM wParam, LPARAM lParam);

SOCKET rectSock; // ������ ����ϱ� ���� ����.

HWND hMainWnd; // �������� hwnd ����Ϸ��� ����.  
HWND hSend; // �����ư
HWND hTextBox; // �αױ��
HWND hChat; // ä��
HINSTANCE g_hinst; // ���� �ν��Ͻ�
//��� �ڵ�� �����ϱ� ���ؼ� ������ ����.

typedef struct dataList{
	RECT rt; //���� ����ü
	char buf[BUFSIZE]; // ä�� ��Ʈ���� �����ֱ� ���ؼ� ������ ����.
}dataList; //�ΰ��� ������ ����ü�� �����ؼ� ����� �Ҷ� ����ϴ� ����.

char buf[BUFSIZE];
vector <RECT> rtList;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
				   LPSTR lpszCmdLine, int nCmdShow)
{
	WSADATA wsa;
	g_hinst = hInstance;
	if (WSAStartup(MAKEWORD(2,2),&wsa))
	{
		return -1;
	}
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
	WndClass.lpszMenuName = NULL;	
	WndClass.lpszClassName = "Window Class Name";	
	RegisterClass(&WndClass);	
	DWORD test = GetCurrentProcessId();
	
	hwnd = CreateWindow("Window Class Name",
		"Ŭ���̾�Ʈ GUI",
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


	WSACleanup();

	return (int)msg.wParam;
}

void DrawRect(HDC hdc, RECT* prt)
{
	Rectangle(hdc,prt->left,prt->top,prt->right,prt->bottom);
}

bool InitRectConnect(short port)
{
	// socket()
	rectSock = socket(AF_INET, SOCK_STREAM, 0);
	if(rectSock == INVALID_SOCKET) return FALSE;	
	
	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	int retval = connect(
        rectSock, //�����ڵ�
        (SOCKADDR *)&serveraddr, //���� ���� �ּҰ�
        sizeof(serveraddr) //�ּҰ� ũ��
        ); // ������ ���� ��û(�����ϸ� �ڵ����� ������Ʈ, �����ּҸ� �Ҵ�)
	if(retval == SOCKET_ERROR) return FALSE;
		
	return true;
}

// ����� ���� ������ ���� �Լ�
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

void RecvRectThread(LPVOID pParam)
	//���� Ŭ���̾�Ʈ���� ������ �׸��ų� ä���� ������.
{
	dataList dl; // ����ü�� �ٽ� ���⼭ ���������� ����ϱ� ���� ����.
	for(;;) // while 
	{
		int retval = recvn(rectSock, (char*)&dl, sizeof(dl), 0); //���� �۾��� �ؾ��ϴ°��̶��
		//������ �ް� ������ �ް� (����)�� �� �� ���⶧����, �����忡���� ��밡���ϴ�.
		if(retval == SOCKET_ERROR){
			break;
		}
		else if(retval == 0){
			break;
		}
		rtList.push_back(dl.rt); //ä�ø� ������� rt���� 0
		if(strlen(dl.buf) > 0) //ä���� �ƴµ� �� ä�� ���̰� 0���� Ŭ��
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)dl.buf); // ä��â�� ����������.
		InvalidateRect(hMainWnd,&dl.rt,TRUE); // ���� ������ ������ ��µǵ���.
	}
	closesocket(rectSock); // Ŭ���̾�Ʈ�� ����Ǹ� Ŭ����.
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, 
						 WPARAM wParam, LPARAM lParam)
{
	HDC			hdc;
	PAINTSTRUCT ps;
	static RECT rt; // ���� rt 
	static RECT rtClient; // �۾� ���� ��ü ũ��
	dataList dl; 
	switch (iMsg) 
	{
	case WM_CREATE:
		GetClientRect(hwnd, &rtClient);
		hMainWnd = hwnd;
		hTextBox=CreateWindow("edit",NULL,WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_THICKFRAME, 0, 0 , 200, 30, hwnd, (HMENU)ID_MESSAGE_BOX,g_hinst,NULL);
		hSend=CreateWindow("button","����", WS_CHILD | WS_VISIBLE | LBS_NOTIFY , 200, 0, 100, 30, hwnd, (HMENU)ID_SEND_BUTTON,g_hinst,NULL);
		hChat=CreateWindow("listbox",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | WS_THICKFRAME, 0, 30, rtClient.right/6, rtClient.bottom/2,hwnd,(HMENU)ID_CHATBOX,g_hinst,NULL);
		SetWindowText(hTextBox,"������ �޼��� �Է�");
		//ä�� �ڵ��� �ϼ��Ǹ� ���۸޽��� �ޱ� ���� ����Ʈ�ڽ��� ���
		if( !InitRectConnect(9000) )	return -1;
		_beginthread(RecvRectThread,0,0); /// ������ �����ǰ� Ŀ��Ʈ�� �� �Ŀ�, ����� ���ú갡 �����ϱ� ������. ���� �Ŀ� �ۼ��Ѵ�.
		break; 
	case WM_COMMAND:
		//������ ä�� â, ä�� �Էºκ�, ��ư,
		switch(LOWORD(wParam))
		{
		case ID_SEND_BUTTON: //���� ��ư
			{
				GetWindowText(hTextBox,buf,512); // ���� ����Ʈ �ڽ��� ���ڿ��� �޾ƿ�.
				RECT rt = {0}; // rt�� 0���� �ʱ�ȭ�ؼ� ������ �����ټ� �ֵ���.
				dl.rt = rt; // rt�� ���� ���� ����ü dl �� �ְ�.
				strcpy(dl.buf,buf); // �迭�̱⶧���� strcpy ����ؼ� dl����ü�� ä�� ���ڿ��� �־���.
				int retval = send(rectSock,(const char*)&dl,sizeof(dl),0); // ����ü ����Ʈ �е� ������ �ѱ涧 ���� �ؾ��Ѵ�. (Byte �迭�� �����, Byte �迭�� �޾ƾ� �Ѵ�.)
				if(retval <= 0 )
				{
					MessageBox(hwnd, "������ send() ����", "MSG", MB_OK);
					break;
				}
			}
			break;
		case ID_MESSAGE_BOX:
			if(HIWORD(wParam) == EN_SETFOCUS) // ���콺�� Ŭ��������.
			{
				SetWindowText(hTextBox,""); 
			}
			break;
		}
		break;
	case WM_LBUTTONDOWN:
		rt.left = LOWORD(lParam);
		rt.top = HIWORD(lParam);
		SetCapture(hwnd);
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		rt.right = LOWORD(lParam);
		rt.bottom = HIWORD(lParam);

		ZeroMemory(&dl, sizeof(dl)); // 0���� �ʱ�ȭ = {0};
		dl.rt =rt; // ĸ���� ������ ����ü�� �����ϰ�.
		
		{
			int retval = send(rectSock,(const char*)&dl,sizeof(dl),0); // ����ü ����Ʈ �е� ������ �ѱ涧 ���� �ؾ��Ѵ�. (Byte �迭�� �����, Byte �迭�� �޾ƾ� �Ѵ�.)
			if(retval <= 0 )
			{
				MessageBox(hwnd, "������ send() ����", "MSG", MB_OK);
				break;
			}
			///������ �����͸� ���� ������ ���� ���Ѵ�. ���ú�� ������(������) ������ ó���� �������� �۾��̴�.
			///���� ������ �����͸� ������ �𸣱� ������ �����带 ����.
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);	
		TextOut(hdc, 0, 0, "HelloWorld", 10);
		for(int i = 0; i < (int ) rtList.size(); ++i)
			DrawRect(hdc,&rtList[i]);
		EndPaint(hwnd, &ps);
		break; 
	case WM_DESTROY:
		closesocket(rectSock);//���� ���� ��������
		PostQuitMessage(0);
		break; 
	}
	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}