#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <fstream>
#include <stdint.h>
#include <ctime>  // 用于获取时间
#include <iomanip> // 用于格式化输出
#include <windows.h>  // 用于 Sleep 函数
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
// 打印接收到的二进制数据
void PrintReceivedData(const std::vector<unsigned char>& data) {
    // 使用传统的 for 循环遍历 std::vector
    for (size_t i = 0; i < data.size(); ++i) {
        // 使用 std::hex 打印十六进制值，并确保打印格式正确
        std::cout << "0x" << std::hex << static_cast<int>(data[i]) << " ";  // 打印为十六进制格式
    }
    std::cout << std::endl;  // 换行
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

// 接收数据（非阻塞接收）
std::vector<unsigned char> ReceiveData(SOCKET clientSock) {
    std::vector<unsigned char> buffer;
    char tempBuffer[16633];

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSock, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms 超时

        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        if (result > 0 && FD_ISSET(clientSock, &readfds)) {
            int bytesReceived = recv(clientSock, tempBuffer, sizeof(tempBuffer), 0);
            
            if (bytesReceived == SOCKET_ERROR) {
				int errCode = WSAGetLastError();
				std::cerr << "Recv failed. Error Code: " << errCode << std::endl;

				// 处理服务器强制关闭连接
				if (errCode == WSAECONNRESET || errCode == WSAENOTCONN || errCode == WSAECONNABORTED) {
					std::cerr << "Server disconnected. Exiting receive loop..." << std::endl;
					closesocket(clientSock);
					stopRequested = true;
					return buffer;
				}
			}

            if (bytesReceived == 0) {  // 服务器主动关闭连接
				std::cerr << "Server closed connection." << std::endl;
				closesocket(clientSock);
				stopRequested = true;
				return buffer;
			}

            buffer.assign(tempBuffer, tempBuffer + bytesReceived);
            return buffer;
        }
    }
    return buffer;
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

// 判断是否为停止指令
bool IsStopCommand(const std::vector<unsigned char>& data) {
    // 使用 push_back 逐个添加元素来初始化 stopCommand
    std::vector<unsigned char> stopCommand;
    stopCommand.push_back(0x2A);
    stopCommand.push_back(0xB2);
    stopCommand.push_back(0xDC);  // 假设 "2AB2DC" 是停止命令的二进制表示

    // 比较数据是否与停止命令匹配
    return std::equal(stopCommand.begin(), stopCommand.end(), data.begin());
}

void WriteDataToFile(const std::vector<unsigned char>& data) {
    std::ofstream outFile("received_data.txt", std::ios::out | std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing." << std::endl;
        return;
    }

    size_t offset = 0;

    // **检查数据长度**
    if (data.size() < (4 * 1024 * 4 + 2 * 60 * 2 + 3 * 3)) {
        std::cerr << "Error: Received data is too short! Expected at least " 
                  << (4 * 1024 * 4 + 2 * 60 * 2 + 3 * 3) 
                  << " bytes, but got " << data.size() << " bytes." << std::endl;
        return;
    }

    // **解析 4 字节整数数据（1024 组）**
    std::vector<int32_t> nearCaptureSpectrum;
    for (size_t i = 0; i < 1024; ++i) {
        if (offset + 4 > data.size()) break;  // 防止越界
        int32_t value = (data[offset] << 24) | (data[offset + 1] << 16) |
                        (data[offset + 2] << 8) | data[offset + 3];
        nearCaptureSpectrum.push_back(value);
        offset += 4;
    }

    std::vector<int32_t> nearNonElasticSpectrum;
    for (size_t i = 0; i < 1024; ++i) {
        if (offset + 4 > data.size()) break;
        int32_t value = (data[offset] << 24) | (data[offset + 1] << 16) |
                        (data[offset + 2] << 8) | data[offset + 3];
        nearNonElasticSpectrum.push_back(value);
        offset += 4;
    }

    std::vector<int32_t> farCaptureSpectrum;
    for (size_t i = 0; i < 1024; ++i) {
        if (offset + 4 > data.size()) break;
        int32_t value = (data[offset] << 24) | (data[offset + 1] << 16) |
                        (data[offset + 2] << 8) | data[offset + 3];
        farCaptureSpectrum.push_back(value);
        offset += 4;
    }

    std::vector<int32_t> farNonElasticSpectrum;
    for (size_t i = 0; i < 1024; ++i) {
        if (offset + 4 > data.size()) break;
        int32_t value = (data[offset] << 24) | (data[offset + 1] << 16) |
                        (data[offset + 2] << 8) | data[offset + 3];
        farNonElasticSpectrum.push_back(value);
        offset += 4;
    }

    // **解析 2 字节整数数据（60 组）**
    std::vector<int16_t> nearTimeSpectrum;
    for (size_t i = 0; i < 60; ++i) {
        if (offset + 2 > data.size()) break;
        int16_t value = (data[offset] << 8) | data[offset + 1];
        nearTimeSpectrum.push_back(value);
        offset += 2;
    }

    std::vector<int16_t> farTimeSpectrum;
    for (size_t i = 0; i < 60; ++i) {
        if (offset + 2 > data.size()) break;
        int16_t value = (data[offset] << 8) | data[offset + 1];
        farTimeSpectrum.push_back(value);
        offset += 2;
    }

    // **解析 3 字节浮点数**
    float generatorVoltage = 0.0f, generatorCurrent = 0.0f, reservedValue = 0.0f;
    if (offset + 3 <= data.size()) {
        generatorVoltage = ((data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]) / 1000.0f;
        offset += 3;
    }
    if (offset + 3 <= data.size()) {
        generatorCurrent = ((data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]) / 1000.0f;
        offset += 3;
    }
    if (offset + 3 <= data.size()) {
        reservedValue = ((data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]) / 1000.0f;
        offset += 3;
    }

    // **写入文件**
    outFile << "Near Capture Spectrum:\n";
    for (size_t i = 0; i < nearCaptureSpectrum.size(); ++i) {
        outFile << nearCaptureSpectrum[i] << " ";
    }
    outFile << "\n\n";

    outFile << "Near Non-Elastic Spectrum:\n";
    for (size_t i = 0; i < nearNonElasticSpectrum.size(); ++i) {
        outFile << nearNonElasticSpectrum[i] << " ";
    }
    outFile << "\n\n";

    outFile << "Far Capture Spectrum:\n";
    for (size_t i = 0; i < farCaptureSpectrum.size(); ++i) {
        outFile << farCaptureSpectrum[i] << " ";
    }
    outFile << "\n\n";

    outFile << "Far Non-Elastic Spectrum:\n";
    for (size_t i = 0; i < farNonElasticSpectrum.size(); ++i) {
        outFile << farNonElasticSpectrum[i] << " ";
    }
    outFile << "\n\n";

    outFile << "Near Time Spectrum:\n";
    for (size_t i = 0; i < nearTimeSpectrum.size(); ++i) {
        outFile << nearTimeSpectrum[i] << " ";
    }
    outFile << "\n\n";

    outFile << "Far Time Spectrum:\n";
    for (size_t i = 0; i < farTimeSpectrum.size(); ++i) {
        outFile << farTimeSpectrum[i] << " ";
    }
    outFile << "\n\n";

    outFile << "Generator Voltage: " << generatorVoltage << "\n";
    outFile << "Generator Current: " << generatorCurrent << "\n";
    outFile << "Reserved Value: " << reservedValue << "\n";
	// 获取当前时间
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    // 格式化时间为 YYYY-MM-DD HH:MM:SS
    outFile << "\n[" << (1900 + localTime->tm_year) << "-"
            << std::setw(2) << std::setfill('0') << (1 + localTime->tm_mon) << "-"
            << std::setw(2) << std::setfill('0') << localTime->tm_mday << " "
            << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
            << std::setw(2) << std::setfill('0') << localTime->tm_min << ":"
            << std::setw(2) << std::setfill('0') << localTime->tm_sec
            << "] Received Data (" << data.size() << " bytes):\n";
    outFile << "-----------------------------------\n";

    outFile.close();
}

// 字符串转换为十六进制字节数组
std::vector<unsigned char> ConvertStringToHex(const std::string& input) {
    std::vector<unsigned char> hexData;
    for (size_t i = 0; i < input.length(); i += 2) {
        std::string byteString = input.substr(i, 2); // 每次取 2 个字符
        unsigned char byte = (unsigned char)strtol(byteString.c_str(), nullptr, 16);
        hexData.push_back(byte);
    }
    return hexData;
}

// 监听用户输入并发送到服务器
DWORD WINAPI UserInputThread(LPVOID lpParam) {
    SOCKET clientSock = *(SOCKET*)lpParam;
    std::string input;

    while (true) {
        std::cout << "Enter command: ";
        std::getline(std::cin, input);  // 读取整行输入

        if (input.length() % 2 != 0) {
            std::cout << "Invalid hex input! Must be even-length." << std::endl;
            continue;
        }

        std::vector<unsigned char> hexCommand = ConvertStringToHex(input);
        if (!hexCommand.empty()) {
			PrintReceivedData(hexCommand);
            SendData(clientSock, hexCommand);
        }
    }
    return 0;
}



int main() {
    // 初始化 Winsock
    if (!InitializeWinsock()) {
        return 1;
    }
	std::string ip_addr ;
	int port;
	std::cout << "输入 IP 地址: ";
	std::cin >> ip_addr;   // 读取 IP 地址

	std::cout << "输入端口号: ";
	std::cin >> port;      // 读取端口号

    // 连接到服务器
    SOCKET clientSock = ConnectToServer(ip_addr, port);  // 假设服务器 IP 是 127.0.0.1，端口是 8080
    if (clientSock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    // 发送初始数据
	//std::vector<unsigned char> startCommand;
	//startCommand.push_back(0x2A); // 0x2A
	//startCommand.push_back(0xA2); // 0xAA
	//startCommand.push_back(0xCC); // 0x2C
    //SendData(clientSock, startCommand);  // 发送特定指令以触发服务器发送数据

	// 启动监听用户输入的线程
    HANDLE hThread = CreateThread(NULL, 0, UserInputThread, &clientSock, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Failed to create input thread." << std::endl;
        return 1;
    }
    CloseHandle(hThread);  // 线程会自己运行，不需要手动等待

    // 持续接收数据
    while (!stopRequested) {  // 监听 stopRequested 变量，用户输入时终止
        std::vector<unsigned char> receivedData = ReceiveData(clientSock);

        if (!receivedData.empty()) {
            WriteDataToFile(receivedData);
        }
        Sleep(100);
    }
	std::cerr << "Client shutting down." << std::endl;
    closesocket(clientSock);
    WSACleanup();

    return 0;
}