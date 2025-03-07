#include <string>
#include <winsock2.h>
#include <iostream>
#include <vector>
#include <windows.h>
#include <fstream>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// 初始化 Winsock
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}

// 创建服务器套接字并开始监听指定端口
SOCKET CreateServerSocket(int port) {
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // 创建一个 TCP 套接字
    if (serverSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有接口
    addr.sin_port = htons(port);  // 绑定到指定端口

    if (bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(serverSock);
        return INVALID_SOCKET;
    }

    if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR) {  // 开始监听
        std::cerr << "Listen failed." << std::endl;
        closesocket(serverSock);
        return INVALID_SOCKET;
    }
    std::cout << "Listening on port " << port << std::endl;
    return serverSock;
}

// 接受客户端连接
SOCKET AcceptClientConnection(SOCKET serverSock) {
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    SOCKET clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);  // 接受客户端连接
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Accept failed." << std::endl;
        return INVALID_SOCKET;
    }

    std::cout << "Client connected: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
    return clientSock;
}

class ClientHandler {
public:
    ClientHandler(SOCKET sock) 
        : clientSock(sock), 
          shouldSendData(false),
          hSendThread(NULL),
          isConnected(true) {}

    ~ClientHandler() {
        Stop(); // 确保资源释放
        closesocket(clientSock);
    }

    void Start() {
        HANDLE hThread = CreateThread(
            NULL, 0, &ClientHandler::HandleClientStatic, this, 0, NULL
        );
        if (hThread) CloseHandle(hThread);
    }

    void Stop() {
        shouldSendData=false;
        if (hSendThread) {
            WaitForSingleObject(hSendThread, INFINITE);
            CloseHandle(hSendThread);
            hSendThread = NULL;
        }
    }

private:
    SOCKET clientSock;
    volatile bool shouldSendData;  // 控制是否发送数据
    HANDLE hSendThread;
    volatile bool isConnected;    // 连接状态

    static DWORD WINAPI HandleClientStatic(LPVOID lpParam) {
        ClientHandler* handler = reinterpret_cast<ClientHandler*>(lpParam);
        handler->HandleClient();
        return 0;
    }

    static DWORD WINAPI SendDataLoopStatic(LPVOID lpParam) {
        ClientHandler* handler = reinterpret_cast<ClientHandler*>(lpParam);
        handler->SendDataLoop();
        return 0;
    }

    void HandleClient() {
        while (isConnected) {
            std::vector<unsigned char> data = ReceiveData();
            if (!data.empty()) {
                PrintReceivedData(data);

                if (IsStartCommand(data)) {
                    std::cout << "Start command received." << std::endl;
                    StartSending();
                } else if (IsStopCommand(data)) {
                    std::cout << "Stop command received." << std::endl;
                    StopSending();
                }
            }
        }
        std::cout << "Client handler exiting." << std::endl;
    }

    void SendDataLoop() {
        std::vector<unsigned char> fileContent = ReadFileContent("./similar_data.txt");
        if (fileContent.empty()) {
            std::cerr << "Failed to read file." << std::endl;
            return;
        }

        while (shouldSendData && isConnected) {
            if (!SendData(fileContent)) {
                // 发送失败，可能连接已断开
                isConnected=false;
                break;
            }
			Sleep(2000);
        }
        shouldSendData=false;
    }

    void StartSending() {
        StopSending(); // 停止之前的发送线程
        shouldSendData=true;
        hSendThread = CreateThread(
            NULL, 0, &ClientHandler::SendDataLoopStatic, this, 0, NULL
        );
        if (!hSendThread) {
            std::cerr << "Failed to create send thread." << std::endl;
            shouldSendData=false;
        }
    }

    void StopSending() {
        shouldSendData=false;
        if (hSendThread) {
            WaitForSingleObject(hSendThread, INFINITE);
            CloseHandle(hSendThread);
            hSendThread = NULL;
        }
    }

    // 其余辅助函数保持原样，只需确保线程安全
    // ReceiveData, SendData, IsStartCommand, IsStopCommand, ReadFileContent 等
    // 示例：修改后的 ReceiveData 函数
    std::vector<unsigned char> ReceiveData() {
        std::vector<unsigned char> buffer(1024);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSock, &readfds);
        struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;  // 超时 100ms

        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        if (result > 0 && FD_ISSET(clientSock, &readfds)) {
            int bytesReceived = recv(clientSock, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
            if (bytesReceived > 0) {
                buffer.resize(bytesReceived);
                return buffer;
            } else {
                // 连接关闭或错误
                isConnected=false;
            }
        }
        return std::vector<unsigned char>();
    }

    // 示例：修改后的 SendData 函数返回是否成功
    bool SendData(const std::vector<unsigned char>& data) {
        int bytesSent = send(clientSock, reinterpret_cast<const char*>(data.data()), data.size(), 0);
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            return false;
        }else {
			std::cout << "Sent " << bytesSent << " bytes of binary data." << std::endl;
			return true;
		}
    }

    // 其他函数如 IsStartCommand, IsStopCommand, ReadFileContent 等保持不变
    // 打印接收到的二进制数据
	void PrintReceivedData(const std::vector<unsigned char>& data) {
		for (size_t i = 0; i < data.size(); ++i) {
			// 使用 std::hex 打印十六进制值，并确保打印格式正确
			std::cout << "0x" << std::hex << static_cast<int>(data[i]) << " ";  // 打印为十六进制格式
		}
		std::cout << std::endl;  // 换行
	}

	// 判断是否为开始指令
	bool IsStartCommand(const std::vector<unsigned char>& data) {
		std::vector<unsigned char> startCommand;//开始命令
		startCommand.push_back(0x2A); // 0x2A
		startCommand.push_back(0xA2); // 0xA2
		startCommand.push_back(0xCC); // 0xCC
		return std::equal(startCommand.begin(), startCommand.end(), data.begin());
	}

	// 判断是否为停止指令
	bool IsStopCommand(const std::vector<unsigned char>& data) {
		std::vector<unsigned char> stopCommand;  // 停止命令
		stopCommand.push_back(0x2A); // 0x2A
		stopCommand.push_back(0xB2); // 0xB2
		stopCommand.push_back(0xDC); // 0xDC
		return std::equal(stopCommand.begin(), stopCommand.end(), data.begin());
	}

	// 从文件读取内容
	std::vector<unsigned char> ReadFileContent(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			std::cerr << "Failed to open file: " << filename << std::endl;
			return std::vector<unsigned char>();
		}

		std::vector<unsigned char> content;
		std::string line;

		while (std::getline(file, line)) {
			for (size_t i = 0; i < line.length(); i += 2) {
				std::string byteStr = line.substr(i, 2);
				unsigned char byte = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
				content.push_back(byte);
			}
		}

		file.close();
		return content;
	}
};

// 其余部分（main函数等）保持原样，只需确保资源释放
int main() {
    if (!InitializeWinsock()) return 1;

    SOCKET serverSock = CreateServerSocket(3000);
    if (serverSock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    while (true) {
        SOCKET clientSock = AcceptClientConnection(serverSock);
        if (clientSock != INVALID_SOCKET) {
            ClientHandler* handler = new ClientHandler(clientSock);
            handler->Start();
            // 注意：实际应用中需要管理 ClientHandler 对象的生命周期
        }
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}