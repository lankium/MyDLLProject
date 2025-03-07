#ifndef SERVER_H
#define SERVER_H

#ifdef SERVERLIB_EXPORTS
    #define SERVERLIB_API __declspec(dllexport)  // 导出符号
#else
    #define SERVERLIB_API __declspec(dllimport)  // 导入符号
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 库

// 初始化 Winsock
extern "C" SERVERLIB_API bool InitializeWinsock();

// 创建客户端套接字并连接到服务器
extern "C" SERVERLIB_API SOCKET ConnectToServer(const std::string& serverIp, int port);

// 发送数据
extern "C" SERVERLIB_API void SendData(SOCKET clientSock, const std::vector<unsigned char>& data);

#endif  // SERVER_H
