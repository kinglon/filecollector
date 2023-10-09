#pragma once

#include <string>
#include <Windows.h>
#include <vector>

class CUtil
{
public:
	// convert unicode string to utf8
	static std::string WstringToUtf8(const std::wstring& wstr);

	// delete folder and its all files
	static bool DeleteFolder(const std::wstring& folderPath);

	// get file size
	static bool GetFileSize(const std::wstring& filePath, ULONGLONG& fileSize);

	// check if the specified file is exist
	static bool IsFileExist(const std::wstring& filePath);

	// list all files in a folder and its sub folder
	static std::vector<std::wstring> ListFiles(const std::wstring& folderPath);
};