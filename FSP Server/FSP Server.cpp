#include "FSP Server.h"
#include "UdpSocket.h"
#include "winsock2.h"
#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include "FspHelper.h"

int main(int argumentCount, char* arguments[])
{
	// Seed rng
	std::srand(std::time(nullptr));

	const std::vector<std::string> args(arguments + 1, arguments + argumentCount);
	uint16_t port = 21;
	uint32_t ip = INADDR_ANY;

	std::string password = "";
	std::filesystem::path path;

	for (int i = 0; i < args.size(); i++) {
		if (!VALID_ARGUMENTS.contains(args[i])) {
			std::cout << "Invalid argument \"" << args[i] << "\" specified." << std::endl;
			return EXIT_FAILURE;
		}

		std::string inputParam;
		std::string inputValue;
		switch (VALID_ARGUMENTS.at(args[i]))
		{
		case PARAM_IGNORE_KEYS:

			inputValue = (++i < args.size() ? args[i] : "");
			if (inputValue == "1") {
				FspClient::checkKeys = false;
			}
			else if (inputValue == "0") {
				FspClient::checkKeys = true;
			}
			else
			{
				std::cout << "Invalid value specified for ignore-keys [1 or 0]";
				return EXIT_SUCCESS;
			}
			break;
		case PARAM_DIRECTORY:
			inputParam = (++i < args.size() ? args[i] : "");
			try
			{
				path.append(inputParam);
				path = std::filesystem::canonical(path);

				if (!std::filesystem::is_directory(path)) {
					throw std::exception("Not a directory");
				}
			}
			catch (const std::exception&)
			{
				std::cout << "Directory does not exist: \"" << path << "\"";
				return EXIT_SUCCESS;
			}
			break;
		case PARAM_VERSION:
			printVersion();
			return EXIT_SUCCESS;
		case PARAM_HELP:
			printHelp();
			return EXIT_SUCCESS;
		case PARAM_PASSWORD:
			password = (++i < args.size() ? args[i] : "");
			break;
		case PARAM_ADDRESS:
			try
			{
				inputValue = (++i < args.size() ? args[i] : "");
				ip = FspHelper::ipStringToUint32(inputValue, port);
			}
			catch (const std::exception&)
			{
				std::cout << "Could not parse ipv4 address";
				return EXIT_SUCCESS;
			}
		}
	}

	if (path == std::filesystem::path()) {
		printHelp();
		return EXIT_SUCCESS;
	}

	std::cout << "Starting server with password \"" << password << "\" in directory \"" << path.string() << "\"" << std::endl;

	UdpSocket::basePath = path;
	UdpSocket client = UdpSocket(ip, port, password);

	while (true) {
		client.listen();
	}
}

void printHelp() {
	std::cout << "Usage: fsp_server.exe -d [directory] [additional options]" << std::endl;
	std::cout << std::noskipws << "  options:" << std::endl;
	std::cout << std::noskipws << "    -d, --directory:         Determines the directory in which the server should run." << std::endl;
	std::cout << std::noskipws << "    -p, --password:          Sets a password that clients need to supply to access files. [Default: none]" << std::endl;
	std::cout << std::noskipws << "    -a, --address:           Determines which address the socket should be bound to. [Default: 0:0.0:0:21]" << std::endl;
	std::cout << std::noskipws << "    -i  -ignore-keys;        Determines whether or not FSP packet key validation should be skipped. [Default: 1]" << std::endl;
	std::cout << std::noskipws << "    -v, --version:           Display version info." << std::endl;
}

void printVersion()
{
	std::cout << std::noskipws << "    _/_/_/                _/_/  _/                                  _/    " << std::endl;
	std::cout << std::noskipws << "     _/    _/_/_/      _/          _/_/_/      _/_/      _/_/_/  _/_/_/_/ " << std::endl;
	std::cout << std::noskipws << "    _/    _/    _/  _/_/_/_/  _/  _/    _/  _/_/_/_/  _/_/        _/      " << std::endl;
	std::cout << std::noskipws << "   _/    _/    _/    _/      _/  _/    _/  _/            _/_/    _/       " << std::endl;
	std::cout << std::noskipws << "_/_/_/  _/    _/    _/      _/  _/    _/    _/_/_/  _/_/_/        _/_/    " << std::endl;
	std::cout << std::endl;
	std::cout << std::noskipws << "FSP Server for Swiss v.1.0 2024-04-18" << std::endl;
}