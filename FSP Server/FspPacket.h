#pragma once
#include <vector>
#include "FspClient.h"
#include <filesystem>
#include <vector>

class FspPacket
{
	struct FspHeader {
		char FSP_COMMAND;
		char MESSAGE_CHECKSUM;
		uint16_t KEY;
		uint16_t SEQUENCE;
		uint16_t DATA_LENGTH;
		uint32_t FILE_POSITION;
	};

	enum Direction {
		FROM_SERVER,
		TO_SERVER
	};

	enum FspCommand {
		CC_VERSION = 0x10,
		CC_ERR = 0x40,
		CC_GET_DIR = 0x41,
		CC_GET_FILE = 0x42,
		CC_UP_LOAD = 0x43,
		CC_INSTALL = 0x44,
		CC_DEL_FILE = 0x45,
		CC_DEL_DIR = 0x46,
		CC_GET_PRO = 0x47,
		CC_SET_PRO = 0x48,
		CC_MAKE_DIR = 0x49,
		CC_BYE = 0x4A,
		CC_GRAB_FILE = 0x4B,
		CC_GRAB_DONE = 0x4C,
		CC_STAT = 0x4D,
		CC_RENAME = 0x4E,
	};

	enum FspProtection {
		OWNER = 0x01,
		DEL = 0x02,
		ADD = 0x04,
		MKDIR = 0x08,
		READ_RESTRICED = 0x10,
		README = 0x20,
		LIST = 0x40,
		RENAME = 0x80
	};

public:
	static std::unique_ptr<FspPacket> createErrorPacket(FspClient fspClient, uint16_t sequence, std::string data, uint16_t errorCode = 0);

	FspPacket(std::vector<char> message);
	FspPacket(FspHeader sentHeader, std::vector<uint8_t> sentData, std::vector<uint8_t> sentExtraData);
	std::vector<char> getRawBytes();
	std::unique_ptr<FspPacket> process(FspClient& fspClient, std::string password);
	char getChecksum(std::vector<char>& message);
	int packetLength();

	FspHeader header;
	std::vector<uint8_t> data;
	std::vector<uint8_t> extraData;

	// Cache data for uploadFile()
	static std::ofstream lastUploadFileStream;
	static std::filesystem::path lastUploadFile;

private:
	Direction direction;

	bool validateToServerChecksum(std::vector<char>& message);
	std::unique_ptr<FspPacket> getDirectoryProtection(FspClient& fspClient);
	std::unique_ptr<FspPacket> getDirectory(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> fileStat(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> getFile(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> rename(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> makeDirectory(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> uploadFile(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> completeUploadFile(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> deleteFile(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> deleteDirectory(FspClient& fspClient, std::string password);
	std::unique_ptr<FspPacket> closeSession(FspClient& fspClient, std::string password);

	// Cache data for getDirectory()
	static uint16_t lastListedPathBlockSize;
	static std::filesystem::path lastListedPath;
	static std::vector<std::vector<uint8_t>> directoryCache;

	// Cache data for getFile()
	static uint16_t lastGetFileBlockSize;
	static std::filesystem::path lastGetFile;
	static std::ifstream lastGetFileStream;
};