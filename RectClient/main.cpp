#include <WinSock2.h>
#include <process.h>
#include <windows.h>
#include <vector>

using std::vector;

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, 
						 WPARAM wParam, LPARAM lParam);

SOCKET sock;
HWND hMainWnd;
vector <RECT> rtList;
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
				   LPSTR lpszCmdLine, int nCmdShow)
{
	WSADATA wsa;

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
	hwnd = CreateWindow("Window Class Name",
		"Ŭ���̾�Ʈ GUi",
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
bool InitConnect(short port)
{
	// socket()
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == INVALID_SOCKET) return FALSE;	
	
	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(9000);
	serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	int retval = connect(
        sock, //�����ڵ�
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
void RecvThread(LPVOID pParam)
{
	RECT rt;
	for(;;)
	{
		int retval = recvn(sock, (char*)&rt, sizeof(RECT), 0); //���� �۾��� �ؾ��ϴ°��̶��
		//������ �ް� ������ �ް� (����)�� �� �� ���⶧����, �����忡���� ��밡���ϴ�.
		if(retval == SOCKET_ERROR){
			break;
		}
		else if(retval == 0){
			break;
		}
		rtList.push_back(rt);
		InvalidateRect(hMainWnd,&rt,TRUE);
	}
	closesocket(sock); // ������ ������ �־�����.
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, 
						 WPARAM wParam, LPARAM lParam)
{
	HDC			hdc;
	PAINTSTRUCT ps;
	static RECT rt;
	switch (iMsg) 
	{
	case WM_CREATE:
		hMainWnd = hwnd;
		if( !InitConnect(9000) )	return -1;
		_beginthread(RecvThread,0,0); /// ������ �����ǰ� Ŀ��Ʈ�� �� �Ŀ�, ����� ���ú갡 �����ϱ� ������. ���� �Ŀ� �ۼ��Ѵ�.
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
		
		{
			int retval = send(sock,(const char*)&rt,sizeof(RECT),0); // ����ü ����Ʈ �е� ������ �ѱ涧 ���� �ؾ��Ѵ�. (Byte �迭�� �����, Byte �迭�� �޾ƾ� �Ѵ�.)
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
		closesocket(sock);//���� ���� ��������
		PostQuitMessage(0);
		break; 
	}
	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}