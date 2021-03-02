// 文件名：BackupRunner.cpp
// 文件功能：BackupHelper存档备份插件--辅助程序
// 作者：yqs112358
// 首发平台：MineBBS
#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <string>
#include <windows.h>
#include <ctime>
#include <locale.h>

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

wstring readConfig(const wstring& sec, const wstring& key, wstring def = L""
    , const unsigned int len = MAX_PATH)
{
    wchar_t* buf = new wchar_t[len + 1];
    wstring res(GetPrivateProfileStringW(sec.c_str(), key.c_str(), def.c_str()
        , buf, len, CONFIG_PATH.c_str()) > 0 ? wstring(buf) : def);
    delete[] buf;
    return res;
}

int readConfig(const wstring& sec, const wstring& key, const int def = 0)
{
    return int(GetPrivateProfileInt(sec.c_str(), key.c_str(), def, CONFIG_PATH.c_str()));
}

bool writeConfig(const wstring& sec, const wstring& key, const wstring value)
{
    return WritePrivateProfileString(sec.c_str(), key.c_str(), value.c_str(), CONFIG_PATH.c_str());
}

void clearTemp()
{
    wchar_t cmdStr[MAX_PATH] = L"rd /S /Q ";
    wcscat_s(cmdStr, TMP_PATH.c_str());

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessW(NULL, cmdStr, NULL, NULL, false, 0, NULL, NULL, &si, &pi))
        WaitForSingleObject(pi.hProcess, MAX_ZIP_WAIT);
}

void failExit(int code)
{
    clearTemp();
    printf("[BackupProcess][FATAL] Backup failed. Error Code: %d", code);
    //Flag to fail
    FILE* fp = _wfopen(END_RESULT.c_str(), L"w");
    fprintf(fp, "%d", code);
    fclose(fp);

    fflush(stdout);
    exit(code);
}

void transStr(wchar_t* str)
{
    do
    {
        if (*str == L'/')
            *str = L'\\';
        if (*str == L'\r' || *str == L'\n')
        {
            *str = L'\0';
            break;
        }
    } while (*str++ != L'\0');
}

bool copyFile(const wstring& fromFile, const wstring& toFile, long long size)
{
    HANDLE fFrom = CreateFile(fromFile.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE , NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fFrom == INVALID_HANDLE_VALUE)
    {
        printf("[BackupProcess][Error] Failed to open Source file!\r\n");
        return false;
    }
    HANDLE fTo = CreateFile(toFile.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, NULL,CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fTo == INVALID_HANDLE_VALUE)
    {
        printf("[BackupProcess][Error] Failed to open Destination file!\r\n");
        return false;
    }
 
    char *buffer = new char[BUFFER_SIZE];
    DWORD readCount, writeCount;
    while (size > 0)
    {
        if (!ReadFile(fFrom, buffer, (size > BUFFER_SIZE) ? BUFFER_SIZE : (DWORD)size, &readCount, NULL))
        {
            printf("[BackupProcess][Error] Failed in reading Source file!\r\n");
            delete[] buffer;
            return false;
        }
        if(!WriteFile(fTo,buffer,readCount,&writeCount,NULL))
        {
            printf("[BackupProcess][Error] Failed in writing Destination file!\r\n");
            delete[] buffer;
            return false;
        }
        size -= readCount;
    }

    CloseHandle(fFrom);
    CloseHandle(fTo);
    delete[] buffer;
    return true;
}

int wmain(int argc,wchar_t ** argv)
{
    //setlocale(LC_ALL, "");

    //Init local config
    if (argc <= 1)
    {
        printf("[BackupProcess][Error] Need config.ini as an argument!\r\n");
        exit(-1);
    }
    else
        CONFIG_PATH = wstring(argv[1]);

    WORLD_PATH = readConfig(L"Core", L"WorldsPath", L".\\worlds\\");
    if (WORLD_PATH[WORLD_PATH.size() - 1] != L'\\')
        WORLD_PATH += L"\\";
    BACKUP_PATH = readConfig(L"Core", L"BackupPath", L".\\backup\\");
    if (BACKUP_PATH[BACKUP_PATH.size() - 1] != L'\\')
        BACKUP_PATH += L"\\";
    TMP_PATH = readConfig(L"Core", L"TempPath", L".\\plugins\\BackupHelper\\temp\\");
    if (TMP_PATH[TMP_PATH.size() - 1] != L'\\')
        TMP_PATH += L"\\";
    ZIP_PATH = readConfig(L"Core", L"7zPath", L".\\plugins\\BackupHelper\\7za.exe");
    END_RESULT = readConfig(L"Core", L"ResultPath", L".\\plugins\\BackupHelper\\end.res");
    RECORD_FILE = readConfig(L"Core", L"BackupList", L"");
    ZIP_LOG_PATH = readConfig(L"Core", L"7zLog", L"");

    BUFFER_SIZE = readConfig(L"Core", L"BufferSize", 16384);
    SHOW_COPY_PROCESS = bool(readConfig(L"Main", L"ShowBackupProcess", 1));
    GROUP_OF_FILES = readConfig(L"Main", L"BackupProcessCount", 10);
    MAX_ZIP_WAIT = readConfig(L"Core", L"MaxWaitForZip", 3600) * 1000;

    if (RECORD_FILE.empty())
    {
        if (argc <= 2)
        {
            printf("[BackupProcess][Error] Need more argument!\r\n");
            failExit(-1);
        }
        else
            RECORD_FILE = wstring(argv[2]);
    }



    //Read record file and Copy Save files
    printf("[BackupProcess] Backup process begin\r\n");

    FILE* fp = _wfopen(RECORD_FILE.c_str(), L"r");
    if (fp == NULL)
    {
        printf("[BackupProcess][Error] Failed to open Record File!\r\n");
        failExit(errno);
    }
    wchar_t filePath[_MAX_PATH];
    long long fileLength;
    wchar_t saveName[_MAX_PATH], * from, * to = saveName;
    int fileCount = 0;
    while (fgetws(filePath, _MAX_PATH, fp) != NULL)
    {
        //Read
        if (filePath[0] == L'\r' || filePath[0] == L'\n')
            break;
        ++fileCount;
        transStr(filePath);

        fwscanf_s(fp, L"%lld", &fileLength);
        fgetwc(fp);      //Skip \n

        //Get save name
        if (fileCount == 1)
        {
            from = filePath;
            while (*from != L'\\')
                *to++ = *from++;
            *to = '\0';
        }
        //Copy
        wstring fromFile = WORLD_PATH + filePath;
        wstring toFile = TMP_PATH + filePath;
        //printf("[BackupProcess] Processing %s\r\n",filePath);
        if (!copyFile(fromFile, toFile, fileLength))
        {
            wprintf(L"[BackupProcess][Error] Failed to copy %ws!\r\n", fromFile.c_str());
            failExit(GetLastError());
        }
        if (SHOW_COPY_PROCESS && fileCount % GROUP_OF_FILES == 0)
            printf("[BackupProcess] %d files processed.\r\n", fileCount);
    }
    fclose(fp);
    printf("[BackupProcess] All of %d files processed.\r\n", fileCount);



    //Construct Cmdline
    printf("[BackupProcess] Files copied. Zipping save files...\r\n");
    printf("[BackupProcess] It may take a long time. Please be patient...\r\n");
    wchar_t timeStr[32];
    time_t nowtime;
    time(&nowtime);
    struct tm* info = localtime(&nowtime);
    wcsftime(timeStr, sizeof(timeStr), L"%Y-%m-%d-%H-%M-%S", info);

    wchar_t cmdStr[_MAX_PATH * 4];
    wsprintf(cmdStr, L"%s a \"%s%s-%s.7z\" \"%s%s\" -sdel -mx1 -mmt",
        ZIP_PATH.c_str(), BACKUP_PATH.c_str(), saveName, timeStr, TMP_PATH.c_str(), saveName);

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
        printf("[BackupProcess][Error] Failed to Zip save files!\r\n");
        failExit(GetLastError());
    }
    WaitForSingleObject(pi.hProcess, MAX_ZIP_WAIT);
    CloseHandle(hZipOutput);

    //Flag to success
    printf("[BackupProcess][Info] Success to Backup.\r\n");
    fp = _wfopen(END_RESULT.c_str(), L"w");
    fputs("0", fp);
    fclose(fp);
    return 0;
}