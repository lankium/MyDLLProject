#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <fstream>
#include <stdint.h>
#include <ctime>  // ���ڻ�ȡʱ��
#include <iomanip> // ���ڸ�ʽ�����
#include <windows.h>  // ���� Sleep ����
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
// ��ӡ���յ��Ķ���������
void PrintReceivedData(const std::vector<unsigned char>& data) {
    // ʹ�ô�ͳ�� for ѭ������ std::vector
    for (size_t i = 0; i < data.size(); ++i) {
        // ʹ�� std::hex ��ӡʮ������ֵ����ȷ����ӡ��ʽ��ȷ
        std::cout << "0x" << std::hex << static_cast<int>(data[i]) << " ";  // ��ӡΪʮ�����Ƹ�ʽ
    }
    std::cout << std::endl;  // ����
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

// �������ݣ����������գ�
std::vector<unsigned char> ReceiveData(SOCKET clientSock) {
    std::vector<unsigned char> buffer;
    char tempBuffer[16633];

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSock, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms ��ʱ

        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        if (result > 0 && FD_ISSET(clientSock, &readfds)) {
            int bytesReceived = recv(clientSock, tempBuffer, sizeof(tempBuffer), 0);
            
            if (bytesReceived == SOCKET_ERROR) {
				int errCode = WSAGetLastError();
				std::cerr << "Recv failed. Error Code: " << errCode << std::endl;

				// ���������ǿ�ƹر�����
				if (errCode == WSAECONNRESET || errCode == WSAENOTCONN || errCode == WSAECONNABORTED) {
					std::cerr << "Server disconnected. Exiting receive loop..." << std::endl;
					closesocket(clientSock);
					stopRequested = true;
					return buffer;
				}
			}

            if (bytesReceived == 0) {  // �����������ر�����
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



// ��������
void SendData(SOCKET clientSock, const std::vector<unsigned char>& data) {
    int bytesSent = send(clientSock, reinterpret_cast<const char*>(data.data()), data.size(), 0);  // ���Ͷ���������
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Send failed." << std::endl;
    } else {
        std::cout << "Sent " << bytesSent << " bytes of binary data." << std::endl;
    }
}

// �ж��Ƿ�Ϊָֹͣ��
bool IsStopCommand(const std::vector<unsigned char>& data) {
    // ʹ�� push_back ������Ԫ������ʼ�� stopCommand
    std::vector<unsigned char> stopCommand;
    stopCommand.push_back(0x2A);
    stopCommand.push_back(0xB2);
    stopCommand.push_back(0xDC);  // ���� "2AB2DC" ��ֹͣ����Ķ����Ʊ�ʾ

    // �Ƚ������Ƿ���ֹͣ����ƥ��
    return std::equal(stopCommand.begin(), stopCommand.end(), data.begin());
}

void WriteDataToFile(const std::vector<unsigned char>& data) {
    std::ofstream outFile("received_data.txt", std::ios::out | std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing." << std::endl;
        return;
    }

    size_t offset = 0;

    // **������ݳ���**
    if (data.size() < (4 * 1024 * 4 + 2 * 60 * 2 + 3 * 3)) {
        std::cerr << "Error: Received data is too short! Expected at least " 
                  << (4 * 1024 * 4 + 2 * 60 * 2 + 3 * 3) 
                  << " bytes, but got " << data.size() << " bytes." << std::endl;
        return;
    }

    // **���� 4 �ֽ��������ݣ�1024 �飩**
    std::vector<int32_t> nearCaptureSpectrum;
    for (size_t i = 0; i < 1024; ++i) {
        if (offset + 4 > data.size()) break;  // ��ֹԽ��
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

    // **���� 2 �ֽ��������ݣ�60 �飩**
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

    // **���� 3 �ֽڸ�����**
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

    // **д���ļ�**
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
	// ��ȡ��ǰʱ��
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    // ��ʽ��ʱ��Ϊ YYYY-MM-DD HH:MM:SS
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

// �ַ���ת��Ϊʮ�������ֽ�����
std::vector<unsigned char> ConvertStringToHex(const std::string& input) {
    std::vector<unsigned char> hexData;
    for (size_t i = 0; i < input.length(); i += 2) {
        std::string byteString = input.substr(i, 2); // ÿ��ȡ 2 ���ַ�
        unsigned char byte = (unsigned char)strtol(byteString.c_str(), nullptr, 16);
        hexData.push_back(byte);
    }
    return hexData;
}

// �����û����벢���͵�������
DWORD WINAPI UserInputThread(LPVOID lpParam) {
    SOCKET clientSock = *(SOCKET*)lpParam;
    std::string input;

    while (true) {
        std::cout << "Enter command: ";
        std::getline(std::cin, input);  // ��ȡ��������

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
    // ��ʼ�� Winsock
    if (!InitializeWinsock()) {
        return 1;
    }
	std::string ip_addr ;
	int port;
	std::cout << "���� IP ��ַ: ";
	std::cin >> ip_addr;   // ��ȡ IP ��ַ

	std::cout << "����˿ں�: ";
	std::cin >> port;      // ��ȡ�˿ں�

    // ���ӵ�������
    SOCKET clientSock = ConnectToServer(ip_addr, port);  // ��������� IP �� 127.0.0.1���˿��� 8080
    if (clientSock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    // ���ͳ�ʼ����
	//std::vector<unsigned char> startCommand;
	//startCommand.push_back(0x2A); // 0x2A
	//startCommand.push_back(0xA2); // 0xAA
	//startCommand.push_back(0xCC); // 0x2C
    //SendData(clientSock, startCommand);  // �����ض�ָ���Դ�����������������

	// ���������û�������߳�
    HANDLE hThread = CreateThread(NULL, 0, UserInputThread, &clientSock, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Failed to create input thread." << std::endl;
        return 1;
    }
    CloseHandle(hThread);  // �̻߳��Լ����У�����Ҫ�ֶ��ȴ�

    // ������������
    while (!stopRequested) {  // ���� stopRequested �������û�����ʱ��ֹ
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