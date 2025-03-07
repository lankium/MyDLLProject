#include <string>
#include <winsock2.h>
#include <iostream>
#include <vector>
#include <windows.h>
#include <fstream>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// ��ʼ�� Winsock
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}

// �����������׽��ֲ���ʼ����ָ���˿�
SOCKET CreateServerSocket(int port) {
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // ����һ�� TCP �׽���
    if (serverSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // �������нӿ�
    addr.sin_port = htons(port);  // �󶨵�ָ���˿�

    if (bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(serverSock);
        return INVALID_SOCKET;
    }

    if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR) {  // ��ʼ����
        std::cerr << "Listen failed." << std::endl;
        closesocket(serverSock);
        return INVALID_SOCKET;
    }
    std::cout << "Listening on port " << port << std::endl;
    return serverSock;
}

// ���ܿͻ�������
SOCKET AcceptClientConnection(SOCKET serverSock) {
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    SOCKET clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);  // ���ܿͻ�������
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
        Stop(); // ȷ����Դ�ͷ�
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
    volatile bool shouldSendData;  // �����Ƿ�������
    HANDLE hSendThread;
    volatile bool isConnected;    // ����״̬

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
                // ����ʧ�ܣ����������ѶϿ�
                isConnected=false;
                break;
            }
			Sleep(2000);
        }
        shouldSendData=false;
    }

    void StartSending() {
        StopSending(); // ֹ֮ͣǰ�ķ����߳�
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

    // ���ศ����������ԭ����ֻ��ȷ���̰߳�ȫ
    // ReceiveData, SendData, IsStartCommand, IsStopCommand, ReadFileContent ��
    // ʾ�����޸ĺ�� ReceiveData ����
    std::vector<unsigned char> ReceiveData() {
        std::vector<unsigned char> buffer(1024);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSock, &readfds);
        struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;  // ��ʱ 100ms

        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        if (result > 0 && FD_ISSET(clientSock, &readfds)) {
            int bytesReceived = recv(clientSock, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
            if (bytesReceived > 0) {
                buffer.resize(bytesReceived);
                return buffer;
            } else {
                // ���ӹرջ����
                isConnected=false;
            }
        }
        return std::vector<unsigned char>();
    }

    // ʾ�����޸ĺ�� SendData ���������Ƿ�ɹ�
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

    // ���������� IsStartCommand, IsStopCommand, ReadFileContent �ȱ��ֲ���
    // ��ӡ���յ��Ķ���������
	void PrintReceivedData(const std::vector<unsigned char>& data) {
		for (size_t i = 0; i < data.size(); ++i) {
			// ʹ�� std::hex ��ӡʮ������ֵ����ȷ����ӡ��ʽ��ȷ
			std::cout << "0x" << std::hex << static_cast<int>(data[i]) << " ";  // ��ӡΪʮ�����Ƹ�ʽ
		}
		std::cout << std::endl;  // ����
	}

	// �ж��Ƿ�Ϊ��ʼָ��
	bool IsStartCommand(const std::vector<unsigned char>& data) {
		std::vector<unsigned char> startCommand;//��ʼ����
		startCommand.push_back(0x2A); // 0x2A
		startCommand.push_back(0xA2); // 0xA2
		startCommand.push_back(0xCC); // 0xCC
		return std::equal(startCommand.begin(), startCommand.end(), data.begin());
	}

	// �ж��Ƿ�Ϊָֹͣ��
	bool IsStopCommand(const std::vector<unsigned char>& data) {
		std::vector<unsigned char> stopCommand;  // ֹͣ����
		stopCommand.push_back(0x2A); // 0x2A
		stopCommand.push_back(0xB2); // 0xB2
		stopCommand.push_back(0xDC); // 0xDC
		return std::equal(stopCommand.begin(), stopCommand.end(), data.begin());
	}

	// ���ļ���ȡ����
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

// ���ಿ�֣�main�����ȣ�����ԭ����ֻ��ȷ����Դ�ͷ�
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
            // ע�⣺ʵ��Ӧ������Ҫ���� ClientHandler �������������
        }
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}