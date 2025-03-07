#ifndef SERVER_H
#define SERVER_H

#ifdef SERVERLIB_EXPORTS
    #define SERVERLIB_API __declspec(dllexport)  // ��������
#else
    #define SERVERLIB_API __declspec(dllimport)  // �������
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")  // ���� Winsock ��

// ��ʼ�� Winsock
extern "C" SERVERLIB_API bool InitializeWinsock();

// �����ͻ����׽��ֲ����ӵ�������
extern "C" SERVERLIB_API SOCKET ConnectToServer(const std::string& serverIp, int port);

// ��������
extern "C" SERVERLIB_API void SendData(SOCKET clientSock, const std::vector<unsigned char>& data);

#endif  // SERVER_H
