#include "UdpClient.h"
#include "FspClient.h"
#include "FspPacket.h"
#include <iostream>
#include <vector>
#include "FspHelper.h"

std::filesystem::path UdpSocket::basePath;
char UdpSocket::messageBuffer[BUFLEN];

UdpSocket::UdpSocket(uint32_t ipAddress, uint16_t port, std::string serverPassword)
{
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
		std::cout << "Failed to initialize winsock. Error code: " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	wSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (wSocket == INVALID_SOCKET) {
		std::cout << "Failed to create UDP socket. Error code: " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	//u_long mode = 1;
	//ioctlsocket(wSocket, FIONBIO, &mode);

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(wSocket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		std::cout << "Could not bind socket. Error code: " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	std::cout << "Sock bind OK" << std::endl;
	std::cout << "Listening on " << FspHelper::uInt32ToIpString(ipAddress, port) << std::endl;

	client = {};
	password = serverPassword;
}

UdpSocket::~UdpSocket()
{
	closesocket(wSocket);
	WSACleanup();
}

void UdpSocket::listen()
{
	// try to receive some data, this is a blocking call
	int clientLength = sizeof(client);
	int receivedBytes = recvfrom(wSocket, messageBuffer, BUFLEN, 0, (sockaddr*)&client, &clientLength);

	// todo change error message
	if (receivedBytes == SOCKET_ERROR) {
		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK) {
			std::cout << "recvfrom() failed with error code: " << error << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	if (0 < receivedBytes) {
		try
		{
			FspPacket received = FspPacket(messageBuffer, receivedBytes);
			FspClient& fspClient = FspClient::getClient(client.sin_addr.s_addr, received.header.KEY);
			FspPacket* responsePacket = received.process(fspClient, password);

			if (responsePacket != nullptr) {
				char* response = responsePacket->getRawBytes();
				sendto(wSocket, response, responsePacket->packetLength(), 0, (sockaddr*)&client, clientLength);
			}

			FspClient::cleanUp();
		}
		catch (std::exception e)
		{
			std::cout << "Error: " << e.what() << std::endl;
		}
	}
}
