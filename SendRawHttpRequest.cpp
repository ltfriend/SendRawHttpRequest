#include <iostream>
#include <string>
#include <fstream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

const char version[] = "1.0";

bool checkParams();
void parseParams(int argc, char* argv[]);
void printHelp();
int sendHttpRequest();

char hostName[100] = "\0";
char hostPort[10] = "80";
char requestFileName[250] = "\0";
char responseFileName[250] = "\0";

struct sendbuffer {
	char* data;
	int len;
};

/// <summary>
/// Usage: sendrawhttprequest host[:port] request_file response_file
/// 
/// Params:
///     host			host name
///		port            host ip port (optional)
///     request_file	text file to send as request
///     response_file   file to save response
///     
/// </summary>
int main(int argc, char* argv[])
{
	std::cout << "Send RAW HTTP request" << std::endl 
		<< "Version: " << version << std::endl << std::endl;

	if (argc == 1) {
		printHelp();
		return 0;
	}

	parseParams(argc, argv);

	if (!checkParams())
	{
		std::cout << "Invalid params. Try typing \"sendrawhttprequest\" for help\n";
		return -1;
	}

	return sendHttpRequest();
}

void printHelp()
{
	std::cout << "Usage: sendrawhttprequest[:port] host request_file response_file\n\n"
		<< "Params:\n"
		<< "    host             host name\n"
		<< "    port             host ip port (optional)\n"
		<< "    request_file     text file to send as request\n"
		<< "    response_file    file to save response\n";
}

constexpr auto PARAM_HOST = 1;
constexpr auto PARAM_REQUEST_FILE = 2;
constexpr auto PARAM_RESPONSE_FILE = 3;

void parseHostName(char* hostFullName)
{
	strcpy_s(hostName, sizeof(hostName), hostFullName);

	char* portDevider = strstr(hostFullName, ":");
	if (portDevider != nullptr)
	{
		int pos = portDevider - hostFullName;
		hostName[pos] = '\0';
		strcpy_s(hostPort, sizeof(hostPort), portDevider + 1);
	}
}

void parseParams(int argc, char* argv[])
{
	for (int paramNum = 1; paramNum < argc; paramNum++) {

		char* param = argv[paramNum];

		switch (paramNum)
		{
		case PARAM_HOST:
			parseHostName(param);
			break;
		case PARAM_REQUEST_FILE:
			strcpy_s(requestFileName, sizeof(requestFileName), param);
			break;
		case PARAM_RESPONSE_FILE:
			strcpy_s(responseFileName, sizeof(responseFileName), param);
			break;
		}

	}
}

bool checkParams()
{
	return (hostName[0] != '\0')
		&& (hostPort[0] != '\0')
		&& (requestFileName[0] != '\0')
		&& (responseFileName[0] != '\0');
}

void deleteSendBuffer(sendbuffer* buffer)
{
	delete[]buffer->data;
	delete buffer;
	buffer = nullptr;
}

sendbuffer* readRequestFile()
{
	std::ifstream requestFile;
	requestFile.open(requestFileName, std::ios::in | std::ios::binary | std::ios::ate);

	if (!requestFile.is_open())
	{
		std::cout << "Error read request file\n";
		return nullptr;
	}

	unsigned int size = (unsigned int)requestFile.tellg();

	sendbuffer* buffer = new sendbuffer();
	buffer->len = size;
	buffer->data = new char[size + 2];
	ZeroMemory(buffer->data, size + 2);

	requestFile.seekg(0, std::ios::beg);
	requestFile.read(buffer->data, size);

	requestFile.close();

	return buffer;
}

constexpr auto RECV_BUFFER_LEN = 1024;

int sendHttpRequest()
{
	sendbuffer* buffer = readRequestFile();
	if (buffer == nullptr)
	{
		return 1;
	}

	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
	{
		std::cout << "WSAStartup failed\n";
		deleteSendBuffer(buffer);
		return 1;
	}

	std::cout << "Sending request...\n";

	struct addrinfo* result = nullptr, * ptr = nullptr, hinst;
	ZeroMemory(&hinst, sizeof(addrinfo));
	hinst.ai_family = AF_UNSPEC;
	hinst.ai_socktype = SOCK_STREAM;
	hinst.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(hostName, hostPort, &hinst, &result);
	if (iResult != 0)
	{
		std::cout << "Get host IP failed\n";
		deleteSendBuffer(buffer);
		WSACleanup();
		return 1;
	}

	ptr = result;
	SOCKET sock = INVALID_SOCKET;
	sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
	if (sock == INVALID_SOCKET)
	{
		std::cout << "Socket creation failed\n";
		deleteSendBuffer(buffer);
		WSACleanup();
		return 1;
	}

	iResult = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "Failed to connect to host\n";
		deleteSendBuffer(buffer);
		WSACleanup();
		return 1;
	}

	iResult = send(sock, buffer->data, buffer->len, 0);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "Request send failed\n";
		deleteSendBuffer(buffer);
		WSACleanup();
		return 1;
	}

	iResult = shutdown(sock, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "Shutdown failed\n";
		deleteSendBuffer(buffer);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	std::cout << "Received response...\n";

	std::ofstream responseFile;
	responseFile.open(responseFileName, std::ios::out | std::ios::binary);

	if (!responseFile.is_open())
	{
		std::cout << "Error create response file\n";
		deleteSendBuffer(buffer);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	char recvBuff[RECV_BUFFER_LEN];

	do
	{
		iResult = recv(sock, recvBuff, RECV_BUFFER_LEN, 0);
		if (iResult > 0)
		{
			responseFile.write(recvBuff, iResult);
		}
		else if (iResult == 0)
		{
			std::cout << "Complete\n";
		}
		else
		{
			std::cout << "Receive response failed\n";
		}
	} while (iResult > 0);

	responseFile.close();
	deleteSendBuffer(buffer);
	closesocket(sock);
	WSACleanup();

	return 0;
}