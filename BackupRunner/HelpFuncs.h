#pragma once
#include <Windows.h>
#include <string>
using std::string;
using std::wstring;

#define CONFIG_PATH_A ".\\plugins\\BackupHelper\\config.ini"
#define CONFIG_PATH L".\\plugins\\BackupHelper\\config.ini"

wstring readConfig(const wstring& sec, const wstring& key, wstring def = L""
            ,const unsigned int len = MAX_PATH)
{
    wchar_t *buf = new wchar_t[len+1];
    wstring res(GetPrivateProfileStringW(sec.c_str(), key.c_str(), def.c_str()
        , buf, len, CONFIG_PATH) > 0 ? wstring(buf) : def);
    delete [] buf;
    return res;
}

string readConfig(const string& sec, const string& key, string def = ""
    , const unsigned int len = MAX_PATH)
{
    char* buf = new char[len + 1];
    string res(GetPrivateProfileStringA(sec.c_str(), key.c_str(), def.c_str()
        , buf, len, CONFIG_PATH_A) > 0 ? string(buf) : def);
    delete[] buf;
    return res;
}

int readConfig(const wstring& sec, const wstring& key, const int def = 0)
{
    return int(GetPrivateProfileInt(sec.c_str(), key.c_str(), def, CONFIG_PATH));
}

bool writeConfig(const wstring& sec, const wstring& key, const wstring value)
{
    return WritePrivateProfileString(sec.c_str(), key.c_str(), value.c_str(), CONFIG_PATH);
}