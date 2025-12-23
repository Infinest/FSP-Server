#pragma once
#pragma comment(lib, "Ws2_32.lib")
#include "winsock2.h"
#include "ws2def.h"
#include "ws2tcpip.h"
#include <string>
#include <filesystem>

#define BUFLEN 1024 * 64

class UdpSocket
{
private:
	WSAData data;
	SOCKET wSocket;
	sockaddr_in server;
	sockaddr_in client;

	static std::vector<char> messageBuffer;
public:
	std::string password;

	UdpSocket(uint32_t ipAddress, uint16_t port, std::string serverPassword);
	~UdpSocket();
	void listen();

	static std::filesystem::path basePath;
};

