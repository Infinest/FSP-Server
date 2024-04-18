#pragma once
#include <cstdint>
#include <filesystem>

class FspDirEnt
{
	enum TYPE {
		TYPE_END = 0x00,
		TYPE_FILE = 0x01,
		TYPE_DIR = 0x02,
		TYPE_SKIP = 0x2A
	};

public:
	static const uint8_t HEADER_SIZE = 9;

	uint32_t time;
	uint32_t size;
	uint8_t type;
	std::string filename;

	FspDirEnt(std::filesystem::directory_entry entry);
	std::vector<uint8_t> getRawBytes(bool includeFilenameAndPadding = true);

	static FspDirEnt getSkipEntry(int padding);
	static FspDirEnt getEndEntry(int padding);
private:
	uint16_t padding = 0;
	FspDirEnt(TYPE setType, int setPadding);
};

