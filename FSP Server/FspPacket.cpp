#include "FspPacket.h"
#include "FspHelper.h"
#include <vector>
#include <stdexcept>
#include "UdpSocket.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <intrin.h>
#include "FspDirEnt.h"
#include <span>

std::vector<std::vector<uint8_t>> FspPacket::directoryCache = {};
std::filesystem::path FspPacket::lastListedPath = std::filesystem::path();
uint16_t FspPacket::lastListedPathBlockSize;

std::ifstream FspPacket::lastGetFileStream;
std::filesystem::path FspPacket::lastGetFile;
uint16_t FspPacket::lastGetFileBlockSize;

std::ofstream FspPacket::lastUploadFileStream;
std::filesystem::path FspPacket::lastUploadFile;

FspPacket::FspPacket(std::vector<char> message)
{
	direction = Direction::TO_SERVER;

	if (message.size() < sizeof(FspHeader)) {
		throw std::invalid_argument("Message length is too small");
	}

	if (!validateToServerChecksum(message)) {
		throw std::invalid_argument("Invalid checksum encountered");
	}

	std::memcpy(&header, message.data(), sizeof(FspHeader));
	header.KEY = _byteswap_ushort(header.KEY);
	header.SEQUENCE = _byteswap_ushort(header.SEQUENCE);
	header.DATA_LENGTH = _byteswap_ushort(header.DATA_LENGTH);
	header.FILE_POSITION = _byteswap_ulong(header.FILE_POSITION);

	if (0 < header.DATA_LENGTH) {
		if ((message.size() - sizeof(header)) < header.DATA_LENGTH) {
			throw std::invalid_argument("Data length is too big");
		}

		auto payload = std::span<const char>(message).subspan(sizeof(header), header.DATA_LENGTH);
		data.assign(payload.begin(), payload.end());
	}
	
	if (sizeof(header) + header.DATA_LENGTH < message.size()) {
		auto extra = std::span<const char>(message).subspan(sizeof(header) + header.DATA_LENGTH);
		extraData.assign(extra.begin(), extra.end());
	}
}

FspPacket::FspPacket(FspHeader sentHeader, std::vector<uint8_t> sentData, std::vector<uint8_t> sentExtraData)
{
	direction = Direction::FROM_SERVER;
	header = sentHeader;
	data = sentData;
	extraData = sentExtraData;

	std::vector<char> rawBytes = getRawBytes();
	header.MESSAGE_CHECKSUM = getChecksum(rawBytes);
}

std::vector<char> FspPacket::getRawBytes()
{
	FspHeader temp = header;
	temp.KEY = _byteswap_ushort(header.KEY);
	temp.SEQUENCE = _byteswap_ushort(header.SEQUENCE);
	temp.DATA_LENGTH = _byteswap_ushort(header.DATA_LENGTH);
	temp.FILE_POSITION = _byteswap_ulong(header.FILE_POSITION);

	int len = packetLength();
	std::vector<char> rawPacket = std::vector<char>(len);
	std::memcpy(rawPacket.data(), &temp, sizeof(temp));
	std::memcpy(rawPacket.data() + sizeof(header), data.data(), data.size());
	std::memcpy(rawPacket.data() + sizeof(header) + data.size(), extraData.data(), extraData.size());

	return rawPacket;
}

FspPacket* FspPacket::process(FspClient& fspClient, std::string password)
{
	if (direction == Direction::FROM_SERVER) {
		throw std::invalid_argument("Packet has not been received from a client");
	}

	switch (header.FSP_COMMAND)
	{
	case FspCommand::CC_GET_PRO:
		return getDirectoryProtection(fspClient);
	case FspCommand::CC_GET_DIR:
		return getDirectory(fspClient, password);
	case FspCommand::CC_STAT:
		return fileStat(fspClient, password);
	case FspCommand::CC_GET_FILE:
		return getFile(fspClient, password);
	case FspCommand::CC_RENAME:
		return rename(fspClient, password);
	case FspCommand::CC_MAKE_DIR:
		return makeDirectory(fspClient, password);
	case FspCommand::CC_UP_LOAD:
		return uploadFile(fspClient, password);
	case FspCommand::CC_INSTALL:
		return completeUploadFile(fspClient, password);
	case FspCommand::CC_DEL_FILE:
		return deleteFile(fspClient, password);
	case FspCommand::CC_DEL_DIR:
		return deleteDirectory(fspClient, password);
	case FspCommand::CC_BYE:
		return closeSession(fspClient, password);
	default:
		std::cout << "Unkown FSP command encountered: " << std::hex << header.FSP_COMMAND;
		break;
	}

	return NULL;
}

char FspPacket::getChecksum(std::vector<char>& message)
{
	if (message.size() < sizeof(FspHeader)) {
		return 0;
	}

	uint32_t actual = 0;
	if (direction == Direction::TO_SERVER) {
		actual = static_cast<uint32_t>(message.size());
	}

	for (size_t i = 0; i < message.size(); ++i) {
		uint8_t byte = (i == 1) ? 0 : static_cast<uint8_t>(message[i]);
		actual += byte;
	}

	actual += actual >> 8;
	return static_cast<char>(actual & 0xFF);
}

int FspPacket::packetLength()
{
	return (sizeof(header) + data.size() + extraData.size());
}

bool FspPacket::validateToServerChecksum(std::vector<char>& message)
{
	char expected = getChecksum(message);
	char actual = message[1];

	return actual == expected;
}


FspPacket* FspPacket::getDirectoryProtection(FspClient& fspClient)
{
	std::vector<uint8_t> sentData = {};
	std::vector<uint8_t> sentExtraData = {
		FspProtection::OWNER | FspProtection::DEL | FspProtection::ADD | FspProtection::MKDIR | FspProtection::READ_RESTRICED | FspProtection::LIST | FspProtection::RENAME
	};

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.MESSAGE_CHECKSUM = 0;
	h.DATA_LENGTH = sentData.size();
	h.FILE_POSITION = sentExtraData.size();

	return new FspPacket(h, sentData, sentExtraData);
}

FspPacket* FspPacket::getDirectory(FspClient& fspClient, std::string password)
{
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);

	FspPacket* error = FspHelper::validatePassword(password, givenPassword, fspClient, header.SEQUENCE);
	if (error != nullptr) {
		return error;
	}

	uint16_t blockSize = 1024;
	if (extraData.size() == 2) {
		uint16_t preferredBlockSize = extraData[1];
		preferredBlockSize += extraData[0] << 8;
		if (0 < preferredBlockSize) {
			blockSize = preferredBlockSize;
		}
	}

	FspHeader h;
	h.FSP_COMMAND = header.FSP_COMMAND;
	h.MESSAGE_CHECKSUM = 0;
	h.KEY = fspClient.key;
	h.SEQUENCE = header.SEQUENCE;
	h.FILE_POSITION = header.FILE_POSITION;

	std::filesystem::path path;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::directory });
	}
	catch (const std::exception& e)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Bad path"));
	}

	if (lastListedPath != path || lastListedPathBlockSize != blockSize) {
		lastListedPathBlockSize = blockSize;
		lastListedPath = path;
		directoryCache.clear();

		std::vector<uint8_t> data = {};
		std::vector<FspDirEnt> entries;
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			entries.push_back(FspDirEnt(entry));
		}

		for (int i = 0; i < entries.size(); i++) {
			try
			{
				std::vector<uint8_t> bytes = entries[i].getRawBytes();
				if (blockSize < data.size() + bytes.size()) {
					if (FspDirEnt::HEADER_SIZE <= blockSize - data.size()) {
						FspDirEnt skip = FspDirEnt::getSkipEntry(blockSize - data.size() - FspDirEnt::HEADER_SIZE);
						std::vector<uint8_t> skipBytes = skip.getRawBytes(false);
						data.insert(data.end(), skipBytes.begin(), skipBytes.end());
					}
					else if (i < entries.size() - 1) {
						data.resize(blockSize, 0);
					}

					directoryCache.push_back(data);
					data.clear();
				}

				// Simply skip the file if the preffered block size is too small
				if (data.size() + bytes.size() <= blockSize)
				{
					data.insert(data.end(), bytes.begin(), bytes.end());
				}
			}
			catch (const std::exception&)
			{
				continue;
			}
		}

		if (data.size() + FspDirEnt::HEADER_SIZE <= blockSize) {
			FspDirEnt end = FspDirEnt::getEndEntry(0);
			std::vector<uint8_t> endBytes = end.getRawBytes();
			data.insert(data.end(), endBytes.begin(), endBytes.end());
		}
		else
		{
			data.resize(blockSize, 0x0);
			directoryCache.push_back(data);
			data.clear();

			FspDirEnt end = FspDirEnt::getEndEntry(0);
			std::vector<uint8_t> endBytes = end.getRawBytes();
			data.insert(data.end(), endBytes.begin(), endBytes.end());
		}

		directoryCache.push_back(data);
	}

	uint16_t key = std::floor(header.FILE_POSITION / blockSize);
	if (directoryCache.size() - 1 < key) {
		h.DATA_LENGTH = 0;
		return new FspPacket(h, {}, {});
	}

	h.DATA_LENGTH = directoryCache[key].size();
	return new FspPacket(h, directoryCache[key], {});
}

FspPacket* FspPacket::fileStat(FspClient& fspClient, std::string password)
{
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);

	FspPacket* error = FspHelper::validatePassword(password, givenPassword, fspClient, header.SEQUENCE);
	if (error != nullptr) {
		return error;
	}

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.MESSAGE_CHECKSUM = 0;
	h.FILE_POSITION = 0;
	h.DATA_LENGTH = FspDirEnt::HEADER_SIZE;

	std::filesystem::path path;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::regular });
	}
	catch (const std::exception&)
	{
		FspDirEnt end = FspDirEnt::getEndEntry(0);
		return new FspPacket(h, end.getRawBytes(false), {});
	}

	std::filesystem::directory_entry e(path);
	FspDirEnt fspDirEnt(e);

	return new FspPacket(h, fspDirEnt.getRawBytes(false), {});
}

FspPacket* FspPacket::deleteDirectory(FspClient& fspClient, std::string password) {
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);

	FspPacket* error = FspHelper::validatePassword(password, givenPassword, fspClient, header.SEQUENCE);
	if (error != nullptr) {
		return error;
	}

	std::filesystem::path path;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::directory, std::filesystem::file_type::not_found });
	}
	catch (const std::exception&)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Delete directory failed"));
	}

	try
	{
		if (std::filesystem::is_directory(path)) {
			std::filesystem::remove_all(path);
			if (!std::filesystem::remove(path))
			{
				throw std::exception("Directory could not be deleted");
			}
		}
	}
	catch (const std::exception&)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Delete directory failed"));
	}

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.DATA_LENGTH = 0;
	h.FILE_POSITION = 0;
	h.MESSAGE_CHECKSUM = 0;

	return new FspPacket(h, {}, {});
}

FspPacket* FspPacket::deleteFile(FspClient& fspClient, std::string password) {
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);

	FspPacket* error = FspHelper::validatePassword(password, givenPassword, fspClient, header.SEQUENCE);
	if (error != nullptr) {
		return error;
	}

	std::filesystem::path path;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::regular, std::filesystem::file_type::not_found });
	}
	catch (const std::exception&)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Delete file failed"));
	}

	try
	{
		if (std::filesystem::is_regular_file(path)) {
			if (!std::filesystem::remove(path))
			{
				throw std::exception("File could not be deleted");
			}
		}
	}
	catch (const std::exception&)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Delete file failed"));
	}

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.DATA_LENGTH = 0;
	h.FILE_POSITION = 0;
	h.MESSAGE_CHECKSUM = 0;

	return new FspPacket(h, {}, {});
}

FspPacket* FspPacket::getFile(FspClient& fspClient, std::string password)
{
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);

	FspPacket* error = FspHelper::validatePassword(password, givenPassword, fspClient, header.SEQUENCE);
	if (error != nullptr) {
		return error;
	}

	uint16_t blockSize = 1024;
	if (extraData.size() == 2) {
		uint16_t preferredBlockSize = extraData[1];
		preferredBlockSize += extraData[0] << 8;
		if (0 < preferredBlockSize) {
			blockSize = preferredBlockSize;
		}
	}

	FspHeader h;
	h.FSP_COMMAND = header.FSP_COMMAND;
	h.MESSAGE_CHECKSUM = 0;
	h.KEY = fspClient.key;
	h.SEQUENCE = header.SEQUENCE;
	h.FILE_POSITION = header.FILE_POSITION;

	std::filesystem::path path;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::regular });
	}
	catch (const std::exception&)
	{
		h.DATA_LENGTH = 0;
		return new FspPacket(h, {}, {});
	}

	if (lastGetFile != path || lastGetFileBlockSize != blockSize || !lastGetFileStream.is_open() || !lastGetFileStream.good()) {
		if (lastGetFileStream.is_open()) {
			lastGetFileStream.close();
		}

		lastGetFile = path;
		lastGetFileBlockSize = blockSize;
		lastGetFileStream = std::ifstream(path, std::ios::binary);
		if (!lastGetFileStream.good()) {
			h.DATA_LENGTH = 0;
			return new FspPacket(h, {}, {});
		}
	}

	lastGetFileStream.seekg(0, std::ios::end);
	auto fileSize = lastGetFileStream.tellg();
	h.DATA_LENGTH = h.FILE_POSITION + blockSize < fileSize ? blockSize : ((uint64_t)fileSize - h.FILE_POSITION);
	std::vector<uint8_t> bytes;
	bytes.resize(h.DATA_LENGTH);
	lastGetFileStream.seekg(h.FILE_POSITION);
	lastGetFileStream.read((char*)bytes.data(), h.DATA_LENGTH);

	return new FspPacket(h, bytes, {});
}

FspPacket* FspPacket::completeUploadFile(FspClient& fspClient, std::string password) {
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);

	FspPacket* error = FspHelper::validatePassword(password, givenPassword, fspClient, header.SEQUENCE);
	if (error != nullptr) {
		return error;
	}

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.DATA_LENGTH = 0;
	h.FILE_POSITION = 0;
	h.MESSAGE_CHECKSUM = 0;

	std::filesystem::path sourcePath = fspClient.getTempFilePath();
	lastUploadFileStream.close();
	if (data.size() == 0) {
		try
		{
			std::filesystem::remove(sourcePath);
		}
		catch (const std::exception&)
		{
			return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Install failed"));
		}
	}

	std::filesystem::path targetPath;
	try
	{
		targetPath = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::not_found, std::filesystem::file_type::regular });
	}
	catch (const std::exception& e)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Install failed"));
	}

	try
	{
		if (std::filesystem::is_regular_file(targetPath)) {
			std::filesystem::remove(targetPath);
		}

		std::filesystem::path directory = targetPath;
		directory.remove_filename();

		std::filesystem::create_directories(directory);
		std::filesystem::rename(sourcePath, targetPath);
	}
	catch (const std::exception& e)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Install failed"));
	}

	return new FspPacket(h, {}, {});
}

FspPacket* FspPacket::uploadFile(FspClient& fspClient, std::string password) {
	std::filesystem::path path = fspClient.getTempFilePath();

	if (lastUploadFile != path || !lastUploadFileStream.is_open() || !lastUploadFileStream.good()) {
		if (lastUploadFileStream.is_open()) {
			lastUploadFileStream.close();
		}

		lastUploadFile = path;
		lastUploadFileStream = std::ofstream(path, std::ios::binary);
		if (!lastUploadFileStream.good()) {
			return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Upload failed"));
		}
	}

	try
	{
		lastUploadFileStream.seekp(header.FILE_POSITION);
		lastUploadFileStream.write((char*)data.data(), data.size());
	}
	catch (const std::exception&) {
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Upload failed"));
	}

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.MESSAGE_CHECKSUM = 0;
	h.DATA_LENGTH = 0;

	return new FspPacket(h, {}, {});
}

FspPacket* FspPacket::rename(FspClient& fspClient, std::string password) {
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);
	std::string givenRenamePassword;
	std::string renameSubPath = FspHelper::getSubPath(extraData, givenRenamePassword);

	std::filesystem::path path;
	std::filesystem::path renamePath;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::directory, std::filesystem::file_type::regular });
		renamePath = FspHelper::getCompletePath(renameSubPath, { std::filesystem::file_type::not_found, std::filesystem::file_type::directory });

		if (std::filesystem::is_directory(path)) {
			std::filesystem::create_directories(renamePath.parent_path());
		}
		else
		{
			auto pathWithoutName = renamePath.parent_path();
			std::filesystem::create_directories(pathWithoutName);
			if (std::filesystem::equivalent(pathWithoutName, lastListedPath))
			{
				lastListedPath = std::filesystem::path();
			}
		}

		std::filesystem::rename(path, renamePath);
	}
	catch (const std::exception&) {}

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.FILE_POSITION = 0;
	h.DATA_LENGTH = 0;
	h.MESSAGE_CHECKSUM = 0;
	return new FspPacket(h, {}, {});
}

FspPacket* FspPacket::makeDirectory(FspClient& fspClient, std::string password) {
	std::string givenPassword;
	std::string subPath = FspHelper::getSubPath(data, givenPassword);
	std::filesystem::path path;
	try
	{
		path = FspHelper::getCompletePath(subPath, { std::filesystem::file_type::not_found , std::filesystem::file_type::directory });
		std::filesystem::create_directories(path);
	}
	catch (const std::exception&)
	{
		return new FspPacket(FspPacket::createErrorPacket(fspClient, header.SEQUENCE, "Could not create directory"));
	}

	std::vector<uint8_t> sentData = {};
	std::vector<uint8_t> sentExtraData = {
		FspProtection::OWNER | FspProtection::DEL | FspProtection::ADD | FspProtection::MKDIR | FspProtection::READ_RESTRICED | FspProtection::LIST | FspProtection::RENAME
	};

	FspHeader h = header;
	h.KEY = fspClient.key;
	h.MESSAGE_CHECKSUM = 0;
	h.DATA_LENGTH = sentData.size();
	h.FILE_POSITION = sentExtraData.size();

	return new FspPacket(h, sentData, sentExtraData);
}

FspPacket* FspPacket::closeSession(FspClient& fspClient, std::string password) {
	fspClient.deleted = true;
	FspHeader h = header;
	h.KEY = fspClient.key;
	h.FILE_POSITION = 0;
	h.DATA_LENGTH = 0;
	h.MESSAGE_CHECKSUM = 0;
	return new FspPacket(h, {}, {});
}

FspPacket FspPacket::createErrorPacket(FspClient fspClient, uint16_t sequence, std::string data, uint16_t errorCode)
{
	std::vector<uint8_t> sentData(data.begin(), data.end());
	sentData.push_back(0x0);
	std::vector<uint8_t> sentExtraData;
	if (0 < errorCode) {
		sentExtraData = { (uint8_t)((errorCode & 0xFF00) >> 8), (uint8_t)(errorCode & 0x00FF) };
	}

	FspHeader h;
	h.FSP_COMMAND = FspCommand::CC_ERR;
	h.KEY = fspClient.key;
	h.SEQUENCE = sequence;
	h.DATA_LENGTH = sentData.size();
	h.FILE_POSITION = sentExtraData.size();

	return FspPacket(h, sentData, sentExtraData);
}