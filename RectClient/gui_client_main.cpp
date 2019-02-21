#include <WinSock2.h>
#include <process.h>
#include <windows.h>
#include <vector>

using std::vector;

#define BUFSIZE 512 //char buf[] 의 크기를 정하기 위해 정의한 것.
#define ID_CHATBOX 101 // 
#define ID_SEND_BUTTON 102
#define ID_MESSAGE_BOX 103


LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, 
						 WPARAM wParam, LPARAM lParam);

SOCKET rectSock; // 서버랑 통신하기 위한 소켓.

HWND hMainWnd; // 전역으로 hwnd 사용하려고 선언.  
HWND hSend; // 샌드버튼
HWND hTextBox; // 로그기록
HWND hChat; // 채팅
HINSTANCE g_hinst; // 전역 인스턴스
//모두 핸들로 관리하기 위해서 선언한 변수.

typedef struct dataList{
	RECT rt; //도형 구조체
	char buf[BUFSIZE]; // 채팅 스트링을 보내주기 위해서 선언한 변수.
}dataList; //두개의 변수를 구조체로 관리해서 통신을 할때 사용하는 단위.

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
		"클라이언트 GUI",
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
        rectSock, //소켓핸들
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

void RecvRectThread(LPVOID pParam)
	//만약 클라이언트에서 도형을 그리거나 채팅을 쳤을때.
{
	dataList dl; // 구조체를 다시 여기서 지역변수로 사용하기 위해 선언.
	for(;;) // while 
	{
		int retval = recvn(rectSock, (char*)&dl, sizeof(dl), 0); //여러 작업을 해야하는것이라면
		//보내고 받고 보내고 받고 (에코)를 할 수 없기때문에, 스레드에서만 사용가능하다.
		if(retval == SOCKET_ERROR){
			break;
		}
		else if(retval == 0){
			break;
		}
		rtList.push_back(dl.rt); //채팅만 쳤을경우 rt값은 0
		if(strlen(dl.buf) > 0) //채팅을 쳤는데 그 채팅 길이가 0보다 클때
			SendMessage(hChat,LB_ADDSTRING,0,(LPARAM)dl.buf); // 채팅창에 보여지도록.
		InvalidateRect(hMainWnd,&dl.rt,TRUE); // 받은 도형이 서버에 출력되도록.
	}
	closesocket(rectSock); // 클라이언트가 종료되면 클로즈.
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, 
						 WPARAM wParam, LPARAM lParam)
{
	HDC			hdc;
	PAINTSTRUCT ps;
	static RECT rt; // 도형 rt 
	static RECT rtClient; // 작업 영역 전체 크기
	dataList dl; 
	switch (iMsg) 
	{
	case WM_CREATE:
		GetClientRect(hwnd, &rtClient);
		hMainWnd = hwnd;
		hTextBox=CreateWindow("edit",NULL,WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_THICKFRAME, 0, 0 , 200, 30, hwnd, (HMENU)ID_MESSAGE_BOX,g_hinst,NULL);
		hSend=CreateWindow("button","전송", WS_CHILD | WS_VISIBLE | LBS_NOTIFY , 200, 0, 100, 30, hwnd, (HMENU)ID_SEND_BUTTON,g_hinst,NULL);
		hChat=CreateWindow("listbox",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | WS_THICKFRAME, 0, 30, rtClient.right/6, rtClient.bottom/2,hwnd,(HMENU)ID_CHATBOX,g_hinst,NULL);
		SetWindowText(hTextBox,"전송할 메세지 입력");
		//채팅 핸들이 완성되면 전송메시지 받기 위해 에디트박스에 출력
		if( !InitRectConnect(9000) )	return -1;
		_beginthread(RecvRectThread,0,0); /// 소켓이 생성되고 커넥트가 된 후에, 샌드와 리시브가 가능하기 떄문에. 연결 후에 작성한다.
		break; 
	case WM_COMMAND:
		//각각의 채팅 창, 채팅 입력부분, 버튼,
		switch(LOWORD(wParam))
		{
		case ID_SEND_BUTTON: //전송 버튼
			{
				GetWindowText(hTextBox,buf,512); // 현재 에디트 박스의 문자열을 받아옴.
				RECT rt = {0}; // rt를 0으로 초기화해서 서버에 보내줄수 있도록.
				dl.rt = rt; // rt를 보낼 단위 구조체 dl 에 넣고.
				strcpy(dl.buf,buf); // 배열이기때문에 strcpy 사용해서 dl구조체에 채팅 문자열을 넣어줌.
				int retval = send(rectSock,(const char*)&dl,sizeof(dl),0); // 구조체 바이트 패딩 때문에 넘길때 주의 해야한다. (Byte 배열로 만들고, Byte 배열로 받아야 한다.)
				if(retval <= 0 )
				{
					MessageBox(hwnd, "서버와 send() 실패", "MSG", MB_OK);
					break;
				}
			}
			break;
		case ID_MESSAGE_BOX:
			if(HIWORD(wParam) == EN_SETFOCUS) // 마우스를 클릭했을때.
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

		ZeroMemory(&dl, sizeof(dl)); // 0으로 초기화 = {0};
		dl.rt =rt; // 캡쳐한 도형을 구조체에 저장하고.
		
		{
			int retval = send(rectSock,(const char*)&dl,sizeof(dl),0); // 구조체 바이트 패딩 때문에 넘길때 주의 해야한다. (Byte 배열로 만들고, Byte 배열로 받아야 한다.)
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
		closesocket(rectSock);//내가 먼저 끊었을때
		PostQuitMessage(0);
		break; 
	}
	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}