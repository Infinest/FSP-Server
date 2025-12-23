#pragma once
#include <vector>
#include <filesystem>
#include "FspPacket.h"
#include <regex>

class FspHelper
{
public:
	static std::string getSubPath(std::vector<uint8_t> data, std::string& outPassword);
	static std::filesystem::path getCompletePath(std::string subPath, std::vector<std::filesystem::file_type> fileTypes);
	static std::unique_ptr<FspPacket> validatePassword(std::string expected, std::string actual, FspClient fspClient, uint16_t sequence);
	static uint32_t fileTimeTypeToUnix(std::filesystem::file_time_type fileTime);
	static uint32_t ipStringToUint32(std::string ipAddress, uint16_t& port);
	static std::string uInt32ToIpString(uint32_t ip, uint16_t port);
private:
	static boolean checkPath(std::filesystem::path base, std::filesystem::path actualPath, std::vector<std::filesystem::file_type> fileTypes);
};

