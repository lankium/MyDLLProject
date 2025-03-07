#include <server.h>
#include <winsock2.h>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>  // ���� CreateThread ����
#include <fstream>    // �����ļ�����
#include <ws2tcpip.h>  // ���� Windows ���纯��

#pragma comment(lib, "ws2_32.lib")  // ���� Winsock ��
volatile bool stopRequested = false;  // C++03 ��֧�� std::atomic������ʹ�� volatile
// ��ʼ�� Winsock
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);  // ���� Winsock ��
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}
// �����ͻ����׽��ֲ����ӵ�������
SOCKET ConnectToServer(const std::string& serverIp, int port) {
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // ����һ�� TCP �׽���
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);  // ���÷������˿�
    inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);  // ���÷����� IP ��ַ

    if (connect(clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect to server failed." << std::endl;
        closesocket(clientSock);
        return INVALID_SOCKET;
    }

    std::cout << "Connected to server: " << serverIp << ":" << port << std::endl;
    return clientSock;
}


// ��������
void SendData(SOCKET clientSock, const std::vector<unsigned char>& data) {
    int bytesSent = send(clientSock, reinterpret_cast<const char*>(data.data()), data.size(), 0);  // ���Ͷ���������
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed." << std::endl;
    } else {
        std::cout << "Sent " << bytesSent << " bytes of binary data." << std::endl;
    }
}
