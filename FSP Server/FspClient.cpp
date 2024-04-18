#include "FspClient.h"
#include "FspPacket.h"
#include <random>
#include <filesystem>
#include <string>
#include <fstream>

std::map<uint32_t, FspClient> FspClient::clients = {};
boolean FspClient::checkKeys = false;

FspClient::FspClient(uint32_t setIpAddress)
{
	deleted = false;
	key = std::rand();
	ipAddress = setIpAddress;
	lastUpdate = std::time(nullptr);
}

boolean FspClient::isOutdated() {
	auto current = std::time(nullptr);
	double difference = difftime(current, lastUpdate);
	return MAX_AFK_TIME < difference;
}

void FspClient::deleteBufferFile() {
	try
	{
		std::filesystem::path tempFile = getTempFilePath();
		if (FspPacket::lastUploadFile == tempFile) {
			FspPacket::lastUploadFileStream.close();
		}

		std::filesystem::remove(getTempFilePath());
	}
	catch (const std::exception&)
	{
	}
}

std::filesystem::path FspClient::getTempFilePath() {
	std::filesystem::path tempPath = std::filesystem::current_path();
	tempPath.append(std::to_string(ipAddress) + ".tmp");
	return tempPath;
}

FspClient& FspClient::getClient(uint32_t ipAddress, uint16_t actualKey)
{
	auto iterator = FspClient::clients.find(ipAddress);
	if (iterator == FspClient::clients.end()) {
		FspClient c(ipAddress);
		FspClient::clients.insert({ ipAddress, c });
		return FspClient::clients.find(ipAddress)->second;
	}
	else
	{
		FspClient& c = iterator->second;
		auto now = std::time(nullptr);
		if (FspClient::checkKeys && c.key != actualKey) {
			double difference = difftime(now, c.lastUpdate);
			if (difference < BAD_KEY_GRACE_TIME) {
				throw std::exception("Bad key");
			}
		}

		c.lastUpdate = now;
		return c;
	}
}

void FspClient::cleanUp() {
	std::map<uint32_t, FspClient>::iterator it = FspClient::clients.begin();
	while (it != FspClient::clients.end())
	{
		if (it->second.deleted || it->second.isOutdated()) {
			it->second.deleteBufferFile();
			it = FspClient::clients.erase(it);
		}
		else
		{
			++it;
		}
	}
}