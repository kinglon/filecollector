#include "Util.h"

std::string CUtil::WstringToUtf8(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();

    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0)
    {       
        return std::string();
    }

    std::string utf8String(utf8Length, '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0)
    {        
        return std::string();
    }

    return utf8String;
}

bool CUtil::DeleteFolder(const std::wstring& folderPath)
{
    WIN32_FIND_DATA findData;
    HANDLE hFind;
    std::wstring path = folderPath + L"\\*";

    hFind = FindFirstFile(path.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0)
            {
                std::wstring filePath = folderPath + L"\\" + findData.cFileName;
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    // Recursively delete subdirectory
                    if (!DeleteFolder(filePath))
                    {
                        FindClose(hFind);
                        return false;
                    }
                }
                else
                {
                    // Delete file
                    if (!DeleteFile(filePath.c_str()))
                    {
                        FindClose(hFind);
                        return false;
                    }
                }
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }

    // Delete the empty folder
    if (!RemoveDirectory(folderPath.c_str()))
    {        
        return false;
    }

    return true;
}

bool CUtil::GetFileSize(const std::wstring& filePath, ULONGLONG& fileSize)
{
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD fileSizeHigh;
    DWORD fileSizeLow = ::GetFileSize(hFile, &fileSizeHigh);
    fileSize = (static_cast<ULONGLONG>(fileSizeHigh) << 32) | fileSizeLow;
    CloseHandle(hFile);
    return true;
}

bool CUtil::IsFileExist(const std::wstring& filePath)
{
    DWORD attributes = GetFileAttributes(filePath.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        return true;
    }

    return false;
}

std::vector<std::wstring> CUtil::ListFiles(const std::wstring& folderPath)
{
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((folderPath + L"\\*").c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return files;
    }

    do
    {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0)
            {
                // Recursively list files in subfolders
                std::wstring subFolderPath = folderPath + L"\\" + findData.cFileName;
                std::vector<std::wstring> subFolderFiles = ListFiles(subFolderPath);
                if (subFolderFiles.size() > 0)
                {
                    files.insert(files.end(), subFolderFiles.begin(), subFolderFiles.end());
                }
            }
        }
        else
        {
            files.push_back(folderPath + L"\\" + findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData) != 0);
    FindClose(hFind);

    return files;
}