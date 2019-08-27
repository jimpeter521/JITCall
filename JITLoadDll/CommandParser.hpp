#pragma once

#include <string>
#include <vector>
class Command
{
public:
	Command(const std::string& command);
	const std::string GetText();
	const std::string GetArg(const int argnum);
	const int GetArgCount();

private:
	std::string _data;
	std::vector<std::string> _tokens;

	enum ParseState
	{
		Default,
		Escaped,
		Text,
		TextEscaped
	};

	void dataFinish();
	void dataAppend(const char ch);
};