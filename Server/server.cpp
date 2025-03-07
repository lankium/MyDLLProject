#include <server.h>
#include <winsock2.h>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>  // 包含 CreateThread 函数
#include <fstream>    // 用于文件操作
#include <ws2tcpip.h>  // 用于 Windows 网络函数

#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 库
volatile bool stopRequested = false;  // C++03 不支持 std::atomic，所以使用 volatile
// 初始化 Winsock
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);  // 启动 Winsock 库
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}
// 创建客户端套接字并连接到服务器
SOCKET ConnectToServer(const std::string& serverIp, int port) {
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // 创建一个 TCP 套接字
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);  // 设置服务器端口
    inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);  // 设置服务器 IP 地址

    if (connect(clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect to server failed." << std::endl;
        closesocket(clientSock);
        return INVALID_SOCKET;
    }

    std::cout << "Connected to server: " << serverIp << ":" << port << std::endl;
    return clientSock;
}


// 发送数据
void SendData(SOCKET clientSock, const std::vector<unsigned char>& data) {
    int bytesSent = send(clientSock, reinterpret_cast<const char*>(data.data()), data.size(), 0);  // 发送二进制数据
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed." << std::endl;
    } else {
        std::cout << "Sent " << bytesSent << " bytes of binary data." << std::endl;
    }
}
