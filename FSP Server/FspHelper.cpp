#include "FspHelper.h"
#include "FspPacket.h"
#include "UdpSocket.h"
#include <regex>
#include <filesystem>
#include <vector>

std::string FspHelper::getSubPath(std::vector<uint8_t> data, std::string& outPassword)
{
	std::string appendage;
	appendage.insert(appendage.end(), data.begin(), data.end());

	size_t position = appendage.find_last_of('\n');
	if (position != std::string::npos) {
		outPassword.insert(outPassword.end(), appendage.begin() + position + 1, appendage.end());
		appendage.erase(appendage.begin() + position, appendage.end());
	}

	return appendage;
}

std::filesystem::path FspHelper::getCompletePath(std::string subPath, std::vector<std::filesystem::file_type> fileTypes)
{
	subPath.erase(0, subPath.find_first_not_of('\\'));
	subPath.erase(0, subPath.find_first_not_of('/'));

	std::filesystem::path base = UdpSocket::basePath;
	std::filesystem::path actualPath = base;
	actualPath.append(subPath);

	auto trimPath = actualPath.generic_string();
	trimPath.erase(trimPath.find_last_not_of('\\') + 1);
	trimPath.erase(trimPath.find_last_not_of('/') + 1);

	actualPath = std::filesystem::absolute(trimPath);

	if (!checkPath(base, actualPath, fileTypes)) {
		throw std::exception("Invalid path specified");
	}

	return actualPath;
}

boolean FspHelper::checkPath(std::filesystem::path base, std::filesystem::path actualPath, std::vector<std::filesystem::file_type> fileTypes)
{
	// Directory validity check
	if (actualPath != base) {
		std::filesystem::path rel = std::filesystem::relative(actualPath, base);
		if (rel.empty() || rel.native()[0] == '.') {
			return false;
		}
	}

	std::filesystem::file_status status = std::filesystem::status(actualPath);
	if (std::find(fileTypes.begin(), fileTypes.end(), status.type()) == fileTypes.end()) {
		return false;
	}

	if ((status.permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
		return false;
	}

	return true;
}

FspPacket* FspHelper::validatePassword(std::string expected, std::string actual, FspClient fspClient, uint16_t sequence)
{
	if (expected.length() == 0) {
		return nullptr;
	}

	if (actual.size() < expected.size()) {
		actual.insert(actual.end(), expected.size() - actual.size(), '\0');
	}
	else if (expected.size() < actual.size())
	{
		expected.insert(expected.end(), actual.size() - expected.size(), '\0');
	}

	bool different = false;
	for (int i = 0; i < expected.length(); i++) {
		different |= expected[i] ^ actual[i];
	}

	if (!different) {
		return nullptr;
	}

	return new FspPacket(FspPacket::createErrorPacket(fspClient, sequence, "Invalid password"));
}

uint32_t FspHelper::fileTimeTypeToUnix(std::filesystem::file_time_type fileTime)
{
	std::chrono::time_point<std::chrono::system_clock> systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
	return std::chrono::system_clock::to_time_t(systemTime);
}

uint32_t FspHelper::ipStringToUint32(std::string ipAddress, uint16_t& port)
{
	uint32_t ip = 0;
	const std::regex ipRegex(R"###(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))(\:([1-9]|[1-5]?[0-9]{2,4}|6[1-4][0-9]{3}|65[1-4][0-9]{2}|655[1-2][0-9]|6553[1-5])|)$)###");
	std::smatch matches;
	auto result = std::regex_search(ipAddress, matches, ipRegex);
	if (!result) {
		throw std::exception("Invalid ipv4 address");
	}

	if (matches.size() == 8) {
		port = static_cast<uint16_t>(std::strtoul(matches[7].str().c_str(), NULL, 0));;
	}

	uint8_t shift = 24;
	uint32_t newIp = 0;
	for (int i = 2; i < 6; i++) {
		uint8_t byte = static_cast<uint8_t>(std::strtoul(matches[i].str().c_str(), NULL, 0));
		ip += byte << shift;
		shift -= 8;
	}

	return ip;
}

std::string FspHelper::uInt32ToIpString(uint32_t ip, uint16_t port)
{
	std::string ipString;
	uint8_t shift = 24;
	while (shift <= 24) {
		if (0 < shift) {
			ipString.append(std::format("{:d}.", (ip >> shift) & 0xFF));
		}
		else {
			ipString.append(std::format("{:d}", (ip >> shift) & 0xFF));
		}

		shift -= 8;
	}

	ipString.append(std::format(":{:d}", port));
	return ipString;
}
