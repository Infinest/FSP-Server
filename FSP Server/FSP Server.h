#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <regex>

const std::regex ipRegex(R"###(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))(\:([1-9]|[1-5]?[0-9]{2,4}|6[1-4][0-9]{3}|65[1-4][0-9]{2}|655[1-2][0-9]|6553[1-5])|)$)###");
const uint8_t PARAM_DIRECTORY = 1;
const uint8_t PARAM_PASSWORD = 2;
const uint8_t PARAM_ADDRESS = 3;
const uint8_t PARAM_VERSION = 4;
const uint8_t PARAM_HELP = 5;
const uint8_t PARAM_IGNORE_KEYS = 6;

const std::map<std::string, uint8_t> VALID_ARGUMENTS = {
	{"-d", PARAM_DIRECTORY},
	{"--directory", PARAM_DIRECTORY},
	{"-p", PARAM_PASSWORD},
	{"--password", PARAM_PASSWORD},
	{"-a", PARAM_ADDRESS},
	{"--address", PARAM_ADDRESS},
	{"-v", PARAM_VERSION},
	{"--version", PARAM_VERSION},
	{"-h", PARAM_HELP},
	{"--help", PARAM_HELP},
	{"-i", PARAM_IGNORE_KEYS},
	{"--ignore-keys", PARAM_IGNORE_KEYS},
};

void printVersion();
void printHelp();