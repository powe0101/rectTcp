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
		"클라이언트 GUi",
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
        sock, //소켓핸들
        (SOCKADDR *)&serveraddr, //접속 서버 주소값
        sizeof(serveraddr) //주소값 크기
        ); // 서버에 접속 요청(성공하면 자동으로 지역포트, 지역주소를 할당)
	if(retval == SOCKET_ERROR) return FALSE;
		
	return true;
}
// 사용자 정의 데이터 수신 함수
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
		int retval = recvn(sock, (char*)&rt, sizeof(RECT), 0); //여러 작업을 해야하는것이라면
		//보내고 받고 보내고 받고 (에코)를 할 수 없기때문에, 스레드에서만 사용가능하다.
		if(retval == SOCKET_ERROR){
			break;
		}
		else if(retval == 0){
			break;
		}
		rtList.push_back(rt);
		InvalidateRect(hMainWnd,&rt,TRUE);
	}
	closesocket(sock); // 서버에 문제가 있었을때.
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
		_beginthread(RecvThread,0,0); /// 소켓이 생성되고 커넥트가 된 후에, 샌드와 리시브가 가능하기 떄문에. 연결 후에 작성한다.
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
			int retval = send(sock,(const char*)&rt,sizeof(RECT),0); // 구조체 바이트 패딩 때문에 넘길때 주의 해야한다. (Byte 배열로 만들고, Byte 배열로 받아야 한다.)
			if(retval <= 0 )
			{
				MessageBox(hwnd, "서버와 send() 실패", "MSG", MB_OK);
				break;
			}
			///서버가 데이터를 언제 줄지는 알지 못한다. 리시브와 윈도우(쓰레드) 데이터 처리는 독립적인 작업이다.
			///언제 서버가 데이터를 보낼지 모르기 때문에 스레드를 띄운다.
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
		closesocket(sock);//내가 먼저 끊었을때
		PostQuitMessage(0);
		break; 
	}
	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}