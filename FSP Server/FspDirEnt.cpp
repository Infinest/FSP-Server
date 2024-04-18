#include "FspDirEnt.h"
#include "FspHelper.h"

FspDirEnt::FspDirEnt(std::filesystem::directory_entry entry)
{
	filename = entry.path().filename().generic_string();
	size = entry.file_size();

	time = FspHelper::fileTimeTypeToUnix(entry.last_write_time());
	std::filesystem::file_status status = entry.status();

	switch (status.type())
	{
	case std::filesystem::file_type::directory:
		type = TYPE::TYPE_DIR;
		break;
	case std::filesystem::file_type::regular:
		type = TYPE::TYPE_FILE;
		break;
	default:
		throw std::exception("Invalid file");
	}
}

FspDirEnt::FspDirEnt(TYPE setType, int setPadding)
{
	size = 0;
	time = 0;
	type = setType;
	padding = setPadding;
}

FspDirEnt FspDirEnt::getSkipEntry(int padding)
{
	return FspDirEnt(TYPE::TYPE_SKIP, padding);
}

FspDirEnt FspDirEnt::getEndEntry(int padding)
{
	return FspDirEnt(TYPE::TYPE_END, padding);
}

std::vector<uint8_t> FspDirEnt::getRawBytes(bool includeFilenameAndPadding)
{
	std::vector<uint8_t> out = {};

	out.push_back((time & 0xFF000000) >> 24);
	out.push_back((time & 0xFF0000) >> 16);
	out.push_back((time & 0xFF00) >> 8);
	out.push_back(time & 0xFF);

	out.push_back((size & 0xFF000000) >> 24);
	out.push_back((size & 0xFF0000) >> 16);
	out.push_back((size & 0xFF00) >> 8);
	out.push_back(size & 0xFF);
	out.push_back(type);

	bool calculatePadding = padding == 0;
	if (includeFilenameAndPadding) {
		out.insert(out.end(), filename.begin(), filename.end());
		padding = 1;
	}

	if (calculatePadding && (out.size() + padding) % 4 != 0) {
		padding += 4 - ((out.size() + padding) % 4);
	}

	out.resize(out.size() + padding, 0);

	return out;
}