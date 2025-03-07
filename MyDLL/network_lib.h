#ifndef NETWORK_LIB_H
#define NETWORK_LIB_H

#ifdef NETWORKLIB_EXPORTS
#define NETWORKLIB_API __declspec(dllexport)  // ��������
#else
#define NETWORKLIB_API __declspec(dllimport)  // �������
#endif

#include <winsock2.h>
#include <string>

// ��ʼ�� Winsock
extern "C" NETWORKLIB_API bool InitializeWinsock();

// ���ӵ�������
extern "C" NETWORKLIB_API SOCKET ConnectToServer(const char* ip, int port);

// ��������
extern "C" NETWORKLIB_API void SendData(SOCKET sock, const std::string& data);

// �������ݣ����������գ�
extern "C" NETWORKLIB_API std::string ReceiveData(SOCKET sock);

// ���ļ���ȡ����
extern "C" NETWORKLIB_API std::string ReadFileContent(const std::string& filename);

// DLL ��������������ͨ��
extern "C" NETWORKLIB_API void StartCommunication(const char* ip, int port, const char* message, const char* stopMessage);

#endif // NETWORK_LIB_H
