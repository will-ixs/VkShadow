#include "logger.h"

Logger::Logger()
{
}

Logger::~Logger()
{
}

void Logger::log(int level, std::string msg) {
	while (level > 0) {
		std::cout << '-';
		level--;
	}
	std::cout << msg << std::endl;
}
void Logger::err(std::string msg) {
	for (int i = 0; i < 20; i++) {
		std::cout << "*";
	}
	throw std::runtime_error(msg);
}