#ifndef NETWORK_LIB_H
#define NETWORK_LIB_H

#ifdef NETWORKLIB_EXPORTS
#define NETWORKLIB_API __declspec(dllexport)  // 导出符号
#else
#define NETWORKLIB_API __declspec(dllimport)  // 导入符号
#endif

#include <winsock2.h>
#include <string>

// 初始化 Winsock
extern "C" NETWORKLIB_API bool InitializeWinsock();

// 连接到服务器
extern "C" NETWORKLIB_API SOCKET ConnectToServer(const char* ip, int port);

// 发送数据
extern "C" NETWORKLIB_API void SendData(SOCKET sock, const std::string& data);

// 接收数据（非阻塞接收）
extern "C" NETWORKLIB_API std::string ReceiveData(SOCKET sock);

// 从文件读取内容
extern "C" NETWORKLIB_API std::string ReadFileContent(const std::string& filename);

// DLL 导出函数：启动通信
extern "C" NETWORKLIB_API void StartCommunication(const char* ip, int port, const char* message, const char* stopMessage);

#endif // NETWORK_LIB_H
