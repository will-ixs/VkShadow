#pragma once
#include <iostream>
#include <string>
class Logger
{
public:
	Logger();
	~Logger();
	void log(int level, std::string msg);
	void err(std::string msg);
};
