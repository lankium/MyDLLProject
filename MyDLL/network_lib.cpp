#include "network_lib.h"
#include <winsock2.h>
#include <iostream>
#include <string>
#include <windows.h>  // 包含 CreateThread 函数
#include <fstream>    // 用于文件操作
#include <ws2tcpip.h>  // 用于 Windows 网络函数

#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 库

// 初始化 Winsock
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);  // 启动 Winsock
    return result == 0;
}

// 连接到服务器
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

// 发送数据
void SendData(SOCKET sock, const std::string& data) {
    int bytesSent = send(sock, data.c_str(), data.length(), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed." << std::endl;
    } else {
        std::cout << "Sent: " << data << std::endl;
    }
}

// 接收数据（非阻塞接收）
std::string ReceiveData(SOCKET sock) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 设置超时 100ms

    int result = select(0, &readfds, nullptr, nullptr, &timeout);
    if (result > 0 && FD_ISSET(sock, &readfds)) {
        char buffer[1024];
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);  // 接收数据
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';  // 添加终止符
            return std::string(buffer);
        }
    }
    return "";  // 没有接收到数据
}

// 从文件读取内容
std::string ReadFileContent(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }

    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";  // 读取每一行并拼接到content中
    }

    file.close();
    return content;
}

// 发送并接收数据的线程
DWORD WINAPI Communicate(LPVOID lpParam) {
    SOCKET sock = (SOCKET)lpParam;
    std::string fileContent = ReadFileContent("similar_data.txt");

    while (true) {
        if (fileContent.empty()) {
            SendData(sock, "Error: File read failed.");
            break;
        }

        SendData(sock, fileContent);  // 发送数据
        Sleep(2000);  // 每 2 秒发送一次

        // 检查是否收到停止指令，若收到则退出发送循环
        std::string receivedData = ReceiveData(sock);
        if (receivedData == "END") {  // 收到停止指令
            std::cout << "Stop command received. Ending response loop." << std::endl;
            break;
        }

        // 检查连接是否关闭
        if (receivedData.empty()) {
            std::cout << "Client disconnected or no data received." << std::endl;
            break;
        }
    }

    closesocket(sock);
    return 0;
}

// DLL 导出函数：启动通信
 void StartCommunication(const char* ip, int port, const char* message, const char* stopMessage) {
    if (!InitializeWinsock()) {
        std::cerr << "Winsock initialization failed." << std::endl;
        return;
    }

    // 连接到服务器
    SOCKET sock = ConnectToServer(ip, port);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to connect to server." << std::endl;
        return;
    }

    // 发送初始消息
    SendData(sock, message);

    // 创建一个线程来处理与服务器的通信（每 2 秒发送一次数据，接收响应）
    HANDLE hThread = CreateThread(NULL, 0, Communicate, (LPVOID)sock, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Error creating thread" << std::endl;
    } else {
        CloseHandle(hThread);  // 不再需要线程句柄
    }
}
