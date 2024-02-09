/**
 * @file logger.hpp
 * @brief Logger class for logging things into a file.
 * @date 2023-04-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

#define RELEASE false
#define LOG_SWITCH true
#define LOG_STDOUT false

constexpr int LOG_LEVEL_DEBUG = 0;
constexpr int LOG_LEVEL_INFO = 1;
constexpr int LOG_LEVEL_WARN = 2;
constexpr int LOG_LEVEL_ERROR = 3;

class Logger {
	public:
		Logger(int _log_level) noexcept;

		const int log_level;

        int round;

		// 输出一条带回合数的日志
		void log(int level, const char* format, ...) noexcept;
        // 无视`log_level`，向stderr输出一条带回合数的日志
		void err(const char* format, ...) noexcept;
        // 无视`log_level`，向stderr输出一条带回合数的日志
		void err(const std::string& str) noexcept;
        // 无视`log_level`，输出一条日志
        void raw(const char* format, ...) noexcept;
        // 若`cond`为真，则输出一条带回合数的警告，返回`cond`本身
		bool warn_if(bool cond, const std::string& str) noexcept;
		// 刷新缓冲区，每回合结束都应调用
		void flush() noexcept;
	private:
		char buffer[256];
		std::FILE* file;
} logger(0);

Logger::Logger(int _log_level) noexcept : log_level(_log_level), round(0) {
	if (LOG_SWITCH) {
		if (LOG_STDOUT) file = stdout;
		else file = stderr;
	}
}
void Logger::log(int level, const char* format, ...) noexcept {
	if (LOG_SWITCH && level >= log_level && !RELEASE) {
		va_list args;
		va_start(args, format);
		vsprintf(buffer, format, args);
		va_end(args);
		fprintf(file, "round%03d: %s\n", round, buffer);
	}
}
void Logger::err(const char* format, ...) noexcept {
	if (!RELEASE) return;
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	fprintf(stderr, "%03d %s\n", round, buffer);
}
void Logger::err(const std::string& str) noexcept {
	if (!RELEASE) return;
	fprintf(stderr, "%03d %s\n", round, str.c_str());
}
void Logger::raw(const char* format, ...) noexcept {
	if (!LOG_SWITCH) return;
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	fprintf(file, "%s", buffer);
}
bool Logger::warn_if(bool cond, const std::string& str) noexcept {
	if (!RELEASE || !cond) return cond;
	fprintf(stderr, "%03d [w] %s\n", round, str.c_str());
	return cond;
}
void Logger::flush() noexcept {
	if (LOG_SWITCH) fflush(file);
}
