// 文件名：BackupRunner.cpp
// 文件功能：BackupHelper存档备份插件--辅助程序
// 作者：yqs112358
// 首发平台：MineBBS
#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <ctime>
#include <cstdio>

using namespace std;

wstring CONFIG_PATH;
wstring WORLD_PATH;
wstring BACKUP_PATH;
wstring TMP_PATH;
wstring ZIP_PATH;
wstring ZIP_LOG_PATH;
wstring END_RESULT;
wstring RECORD_FILE;
int BUFFER_SIZE;
bool SHOW_COPY_PROCESS;
int GROUP_OF_FILES;
int MAX_ZIP_WAIT;

wstring ReadConfig(const wstring& sec, const wstring& key, wstring def = L""
    , const unsigned int len = MAX_PATH)
{
    wchar_t* buf = new wchar_t[len + 1];
    wstring res(GetPrivateProfileStringW(sec.c_str(), key.c_str(), def.c_str()
        , buf, len, CONFIG_PATH.c_str()) > 0 ? wstring(buf) : def);
    delete[] buf;
    return res;
}

int ReadConfig(const wstring& sec, const wstring& key, const int def = 0)
{
    return int(GetPrivateProfileInt(sec.c_str(), key.c_str(), def, CONFIG_PATH.c_str()));
}

bool WriteConfig(const wstring& sec, const wstring& key, const wstring value)
{
    return WritePrivateProfileString(sec.c_str(), key.c_str(), value.c_str(), CONFIG_PATH.c_str());
}

wstring U8str2wchar(const string& u8str)
{
    size_t targetLen = MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, NULL, 0);
    if (targetLen == ERROR_NO_UNICODE_TRANSLATION || targetLen == 0)
        return L"";
    wchar_t *res = new wchar_t[targetLen+1];
    ZeroMemory(res, sizeof(wchar_t) * (targetLen + 1));
    size_t resLen = MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, res, targetLen);
    wstring resStr = wstring(res);

    delete[] res;
    return resStr;
}


bool DelFile(wstring file)
{
    return DeleteFile(file.c_str());
}

bool DelDir(wstring dir)
{
    if (dir.back() == L'\\')
        dir.pop_back();
    WIN32_FIND_DATA findFileData;
    wstring dirFind = dir + L"\\*";

    HANDLE hFind = FindFirstFile(dirFind.c_str(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
        return false;
    do
    {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wcscmp(findFileData.cFileName, L".") != 0
                && wcscmp(findFileData.cFileName, L"..") != 0)
            {
                wstring target = dir + L"\\" + findFileData.cFileName;
                DelDir(target);
            }
        }
        else
            DelFile(dir + L"\\" + findFileData.cFileName);
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);
    RemoveDirectory(dir.c_str());
    return true;
}

void FailExit(int code)
{
    DelDir(TMP_PATH);
    cout << "[BackupProcess][FATAL] Backup failed. Error Code: " << code << endl;
    //Flag to fail
    ofstream fout(END_RESULT);
    fout << code;
    fout.close();

    fflush(stdout);
    exit(code);
}

int ClearOldBackup(wstring beforeTimestamp)
{
    unsigned long long timeStamp = _wtoll(beforeTimestamp.c_str());
    wstring dirFind = BACKUP_PATH + L"*";
    WIN32_FIND_DATA findFileData;
    ULARGE_INTEGER createTime;
    int clearCount = 0;

    HANDLE hFind = FindFirstFile(dirFind.c_str(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        cout << "[BackupProcess][Warning] Fail to locate old backups." << endl;
        return GetLastError();
    }
    do
    {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        else
        {
            createTime.LowPart = findFileData.ftCreationTime.dwLowDateTime;
            createTime.HighPart = findFileData.ftCreationTime.dwHighDateTime;
            if (createTime.QuadPart / 10000000 - 11644473600 < timeStamp)
            {
                DelFile(BACKUP_PATH + findFileData.cFileName);
                ++clearCount;
            }
        }
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);
    if(clearCount > 0)
        cout << "[BackupProcess][Info] " << clearCount << " old backups cleaned." << endl;
    return 0;
}

int wmain(int argc,wchar_t ** argv)
{
    //Read local config
    if (argc <= 1)
    {
        cout << "[BackupProcess][Error] Need config.ini as an argument!" << endl;
        exit(-1);
    }
    else
        CONFIG_PATH = wstring(argv[1]);

    // Clear old backups
    BACKUP_PATH = ReadConfig(L"Core", L"BackupPath", L".\\backup\\");
    if (BACKUP_PATH.back() != L'\\')
        BACKUP_PATH += L"\\";
    if (argc == 4 && wcscmp(argv[2], L"-c") == 0)
    {
        return ClearOldBackup(argv[3]);
    }

    // Get Record file
    RECORD_FILE = ReadConfig(L"Core", L"BackupList", L"");
    if (RECORD_FILE.empty())
    {
        if (argc <= 2)
        {
            cout << "[BackupProcess][Error] Need more argument!" << endl;
            FailExit(-1);
        }
        else
            RECORD_FILE = wstring(argv[2]);
    }

    WORLD_PATH = ReadConfig(L"Core", L"WorldsPath", L".\\worlds\\");
    if (WORLD_PATH.back() != L'\\')
        WORLD_PATH += L"\\";
    TMP_PATH = ReadConfig(L"Core", L"TempPath", L".\\plugins\\BackupHelper\\temp\\");
    if (TMP_PATH.back() != L'\\')
        TMP_PATH += L"\\";
    ZIP_PATH = ReadConfig(L"Core", L"7zPath", L".\\plugins\\BackupHelper\\7za.exe");
    END_RESULT = ReadConfig(L"Core", L"ResultPath", L".\\plugins\\BackupHelper\\end.res");
    
    ZIP_LOG_PATH = ReadConfig(L"Core", L"7zLog", L"");
    BUFFER_SIZE = ReadConfig(L"Core", L"BufferSize", 16384);
    SHOW_COPY_PROCESS = bool(ReadConfig(L"Main", L"ShowBackupProcess", 1));
    GROUP_OF_FILES = ReadConfig(L"Main", L"BackupProcessCount", 50);
    MAX_ZIP_WAIT = ReadConfig(L"Core", L"MaxWaitForZip", 3600) * 1000;



    //Read record file and Copy Save files
    cout << "[BackupProcess] Backup process begin" << endl;

    ifstream fin(RECORD_FILE);
    if (!fin)
    {
        cout << "[BackupProcess][Error] Failed to open Record File!" << endl;
        cout << strerror(errno) << endl;
        FailExit(errno);
    }

    string tmp;
    wstring filePath;
    long long fileLength;
    wstring saveName;
    int fileCount = 0;

    cout << "[BackupProcess] Copying files..." << endl;
    while (getline(fin, tmp))
    {
        //Read Path
        if (tmp.empty())
            break;
#ifdef _DEBUG
        cout << "[DEBUG] READ LINE: " << tmp << endl;
#endif
        filePath = U8str2wchar(tmp);
        if (filePath.empty())
        {
            wcout << L"[BackupProcess][Error] Failed to transform from UTF-8!" << endl;
            FailExit(GetLastError());
        }
        ++fileCount;

        // change /
        for (size_t i = 0; i < filePath.size(); ++i)
            if (filePath[i] == L'/')
                filePath[i] = L'\\';

        //Read Length
        fin >> fileLength;
        getline(fin, tmp);

        //Get save name
        if (fileCount == 1)
        {
            saveName = filePath.substr(0, filePath.find(L'\\'));
#ifdef _DEBUG
            wcout << L"[DEBUG] SaveName: " << filePath << endl;
#endif
        }

        //Copy
        wstring fromFile = WORLD_PATH + filePath;
        wstring toFile = TMP_PATH + filePath;
#ifdef _DEBUG
        wcout << L"[BackupProcess] Processing " << filePath << endl;
#endif
        if (!CopyFile(fromFile.c_str(), toFile.c_str(),false))
        {
            wcout << L"[BackupProcess][Error] Failed to copy " << fromFile << L"!" << endl;
            FailExit(GetLastError());
        }

        //Truncate
        LARGE_INTEGER pos;
        pos.QuadPart = fileLength;
        LARGE_INTEGER curPos;
        HANDLE hSaveFile = CreateFileW(toFile.c_str(), GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, 0);
        if (hSaveFile == INVALID_HANDLE_VALUE
            || !SetFilePointerEx(hSaveFile, pos, &curPos, FILE_BEGIN)
            || !SetEndOfFile(hSaveFile))
        {
            wcout << L"[BackupProcess][Error] Failed to truncate " << fromFile << L"!" << endl;
            FailExit(GetLastError());
        }
        CloseHandle(hSaveFile);
        if (SHOW_COPY_PROCESS && fileCount % GROUP_OF_FILES == 0)
            cout << "[BackupProcess] " << fileCount << " files processed." << endl;
    }
    fin.close();
    cout << "[BackupProcess] All of " << fileCount << " files processed." << endl;



    //Construct Cmdline
    cout << "[BackupProcess] Files copied. Zipping save files..." << endl;
    cout << "[BackupProcess] It may take a long time. Please be patient..." << endl;
    wchar_t timeStr[32];
    time_t nowtime;
    time(&nowtime);
    struct tm* info = localtime(&nowtime);
    wcsftime(timeStr, sizeof(timeStr), L"%Y-%m-%d-%H-%M-%S", info);

    wchar_t cmdStr[_MAX_PATH * 4];
    wsprintf(cmdStr, L"%ls a \"%ls%ls-%ls.7z\" \"%ls%ls\" -sdel -mx1 -mmt"
        ,ZIP_PATH.c_str(), BACKUP_PATH.c_str(), saveName.c_str(), timeStr
        , TMP_PATH.c_str(), saveName.c_str());
#ifdef _DEBUG
    wcout << L"[DEBUG] CMD LINE: " << cmdStr << endl;
#endif

    //Prepare for output
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES),NULL,TRUE };
    HANDLE hZipOutput = INVALID_HANDLE_VALUE;
    if (!ZIP_LOG_PATH.empty())
    {
        hZipOutput = CreateFile(ZIP_LOG_PATH.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE , &sa,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hZipOutput != INVALID_HANDLE_VALUE)
        {
            si.hStdOutput = hZipOutput;
            si.hStdError = hZipOutput;
        }
        else
            si.hStdOutput = si.hStdError = INVALID_HANDLE_VALUE;
    }
    else
        si.hStdOutput = si.hStdError = INVALID_HANDLE_VALUE;
        
    si.dwFlags = STARTF_USESTDHANDLES;

    //Create zip process
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, cmdStr, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        cout << "[BackupProcess][Error] Failed to Zip save files!" << endl;
        FailExit(GetLastError());
    }
    WaitForSingleObject(pi.hProcess, MAX_ZIP_WAIT);
    CloseHandle(hZipOutput);

    //Flag to success
    cout << "[BackupProcess][Info] Success to Backup." << endl;
    ofstream fout(END_RESULT);
    fout.put('0');
    fout.close();
    return 0;
}