#pragma once
#include <map>
#include <winsock2.h>
#include <filesystem>
class FspClient
{
public:
	uint16_t key;
	uint32_t ipAddress;
	bool deleted;
	std::time_t lastUpdate;

	FspClient(uint32_t setIpAddress);
	std::filesystem::path getTempFilePath();
	void deleteBufferFile();
	boolean isOutdated();

	static boolean checkKeys;

	static void cleanUp();
	static FspClient& getClient(uint32_t ipAddress, uint16_t actualKey);
	static std::map<uint32_t, FspClient> clients;

private:
	static const uint16_t MAX_AFK_TIME = 5 * 60;
	static const uint8_t BAD_KEY_GRACE_TIME = 60;
};

