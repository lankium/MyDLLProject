#include "network_lib.h"
#include <winsock2.h>
#include <iostream>
#include <string>
#include <windows.h>  // ���� CreateThread ����
#include <fstream>    // �����ļ�����
#include <ws2tcpip.h>  // ���� Windows ���纯��

#pragma comment(lib, "ws2_32.lib")  // ���� Winsock ��

// ��ʼ�� Winsock
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);  // ���� Winsock
    return result == 0;
}

// ���ӵ�������
SOCKET ConnectToServer(const char* ip, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed." << std::endl;
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

// ��������
void SendData(SOCKET sock, const std::string& data) {
    int bytesSent = send(sock, data.c_str(), data.length(), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed." << std::endl;
    } else {
        std::cout << "Sent: " << data << std::endl;
    }
}

// �������ݣ����������գ�
std::string ReceiveData(SOCKET sock) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // ���ó�ʱ 100ms

    int result = select(0, &readfds, nullptr, nullptr, &timeout);
    if (result > 0 && FD_ISSET(sock, &readfds)) {
        char buffer[1024];
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);  // ��������
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';  // �����ֹ��
            return std::string(buffer);
        }
    }
    return "";  // û�н��յ�����
}

// ���ļ���ȡ����
std::string ReadFileContent(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }

    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";  // ��ȡÿһ�в�ƴ�ӵ�content��
    }

    file.close();
    return content;
}

// ���Ͳ��������ݵ��߳�
DWORD WINAPI Communicate(LPVOID lpParam) {
    SOCKET sock = (SOCKET)lpParam;
    std::string fileContent = ReadFileContent("similar_data.txt");

    while (true) {
        if (fileContent.empty()) {
            SendData(sock, "Error: File read failed.");
            break;
        }

        SendData(sock, fileContent);  // ��������
        Sleep(2000);  // ÿ 2 �뷢��һ��

        // ����Ƿ��յ�ָֹͣ����յ����˳�����ѭ��
        std::string receivedData = ReceiveData(sock);
        if (receivedData == "END") {  // �յ�ָֹͣ��
            std::cout << "Stop command received. Ending response loop." << std::endl;
            break;
        }

        // ��������Ƿ�ر�
        if (receivedData.empty()) {
            std::cout << "Client disconnected or no data received." << std::endl;
            break;
        }
    }

    closesocket(sock);
    return 0;
}

// DLL ��������������ͨ��
 void StartCommunication(const char* ip, int port, const char* message, const char* stopMessage) {
    if (!InitializeWinsock()) {
        std::cerr << "Winsock initialization failed." << std::endl;
        return;
    }

    // ���ӵ�������
    SOCKET sock = ConnectToServer(ip, port);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to connect to server." << std::endl;
        return;
    }

    // ���ͳ�ʼ��Ϣ
    SendData(sock, message);

    // ����һ���߳����������������ͨ�ţ�ÿ 2 �뷢��һ�����ݣ�������Ӧ��
    HANDLE hThread = CreateThread(NULL, 0, Communicate, (LPVOID)sock, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Error creating thread" << std::endl;
    } else {
        CloseHandle(hThread);  // ������Ҫ�߳̾��
    }
}
