#include <winsock2.h>
#include <process.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>

using std::vector;

vector <RECT> rtList;
vector <SOCKET> sockVec;

HANDLE hMutex;

#define BUFSIZE 512


void SyncPushSocket(const SOCKET client_sock)
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
	printf("[%s] %s", msg, (LPCTSTR)lpMsgBuf);
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
{
	int retval = 0;
	WaitForSingleObject(hMutex,INFINITE);
	for(unsigned int i = 0; i< rtList.size(); ++i)
	{
		retval = send(client_sock, (const char*)&rtList[i], sizeof(rtList[i]), 0); 
		/// ���� ������ Ŭ���̾�Ʈ ���� ����ȭ
		if(retval == SOCKET_ERROR){
			err_display("send()");
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
	printf("[TCP ����] ���� �������� ��� Ŭ���̾�Ʈ�� �׸� ������ �����մϴ�.\n");
	for(unsigned int i = 0; i< sockVec.size(); ++i)
	{
		SOCKET currentSocket = sockVec[i];// ��� Ŭ���̾�Ʈ ���� ����ȭ

		retval = send(currentSocket, (char*)&rt, sizeof(RECT), 0); 
		if(retval == SOCKET_ERROR){
			err_display("send()");
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
	// Ŭ���̾�Ʈ ���� ���
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (SOCKADDR *)&clientaddr, &addrlen);

	SyncInitRect(client_sock);

	while(1){

		// ������ �ޱ�
		retval = recvn(client_sock, (char*)&rt, sizeof(RECT), 0);
		if(retval == SOCKET_ERROR){
			err_display("recv()");
			break;
		}
		else if(retval == 0){
			break;
		}

		SyncPushRect(rt);

		// ���� ������ ���
		printf("[TCP/%s:%d] %d,%d,%d,%d\n", inet_ntoa(clientaddr.sin_addr),
			ntohs(clientaddr.sin_port), rt.left,rt.top,rt.right,rt.bottom);
		// ������ ������
		SyncSendRect(rt);

		// closesocket()
		
	}
	closesocket(client_sock);

	SyncDeleteSocket(client_sock);

	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d, ���� �������� Ŭ���̾�Ʈ : %d\n", 
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port),sockVec.size());
	return 0;

}
int main(int argc, char* argv[])
{
	int retval;
	hMutex = CreateMutex(NULL,FALSE,NULL);
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return -1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
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

	// ������ ��ſ� ����� ����
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
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d, ���� ������ Ŭ���̾�Ʈ �� : %d\n", 
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), sockVec.size());

		// ������ ����
		hThread = (HANDLE)_beginthreadex(NULL, 0, ProcessClient, 
			(LPVOID)client_sock, 0, &ThreadId);

		if(hThread == NULL)
			printf("[����] ������ ���� ����!\n");
		else
			CloseHandle(hThread);
	}

	// closesocket()
	closesocket(listen_sock);
	CloseHandle(hMutex);
	// ���� ����
	WSACleanup();
	return 0;
}