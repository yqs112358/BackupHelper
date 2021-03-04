// 文件名：BackupHelper.net.js
// 文件功能：NetJs平台下BackupHelper存档备份插件
// 作者：yqs112358
// 首发平台：MineBBS

var _VER = '1.3.2';
var _CONFIG_FILE = '.\\plugins\\BackupHelper\\config.ini'

var waitingRecord = false;
var isBackuping = false;
var autoBakEnabled = false;

function _LOG(e) {
    log('[BackupHelper] ' + e);
}

function getConfigInt(configStr,configName)
{
    if(configStr.endsWith('\r'))
        configStr = configStr.substr(0,configStr.length-1);
    if(configStr.startsWith(configName+'=') && configStr.length > configName.length+1)
        return Number(configStr.substr(configName.length+1));
    else
        return null;
}

function getConfigStr(configStr,configName)
{
    if(configStr.endsWith('\r'))
        configStr = configStr.substr(0,configStr.length-1);
    if(configStr.startsWith(configName+'=') && configStr.length > configName.length+1)
        return configStr.substr(configName.length+1);
    else
        return null;
}

function buildDirTree(dest) {
    let dirList = dest.split('/');
    let nowPath = _TEMP_PATH;
    for (let i = 0; i < dirList.length - 1; i++) {
        nowPath = nowPath + '\\' + dirList[i];
        if (!mkdir(nowPath))
            return false;
    }
    return true;
}

function initLocalConfig() {
    mkdir('.\\plugins');
    mkdir('.\\plugins\\BackupHelper');

    let configStr = fileReadAllText(_CONFIG_FILE);
    if(configStr != null && configStr != "")
    {
        let configs = configStr.split("\n");
        let tmpStr = '',tmpInt = 0;
        for(let i = 0; i < configs.length; i++)
        {
            tmpStr = getConfigStr(configs[i],'BackupList');
            if(tmpStr != null)
                _BACKUP_LIST = tmpStr;
            tmpStr = getConfigStr(configs[i],'RunnerPath');
            if(tmpStr != null)
                _BACKUP_RUNNER = tmpStr;
            tmpStr = getConfigStr(configs[i],'TempPath');
            if(tmpStr != null)
                _TEMP_PATH = tmpStr;
            tmpStr = getConfigStr(configs[i],'ResultPath');
            if(tmpStr != null)
                _BACKUP_RESULT = tmpStr;
            
            tmpInt = getConfigInt(configs[i],'MaxStorageTime');
            if(tmpInt != null)
                _MAX_SAVE_TIME = tmpInt;
            tmpInt = getConfigInt(configs[i],'AutoBackupInterval');
            if(tmpInt != null)
                _AUTO_BAK_INTERVAL = tmpInt;
        }
        if(_AUTO_BAK_INTERVAL > 0)
        {
            autoBakEnabled = true;
            setTimeout(autoBackup,_AUTO_BAK_INTERVAL*3600000);
            _LOG('【自动备份已启动】');
            _LOG('距下一次自动备份还有'+_AUTO_BAK_INTERVAL+'小时');
        }
        else if(autoBakEnabled)
        {
            autoBakEnabled = false;
            _LOG('【自动备份已关闭】');
        }
        _LOG('配置文件加载成功')
    }
}

function waitForSave() {
    let res = runcmd('save query')
    if (!res) {
        _LOG('[Error] 存档备份数据获取失败！');
        waitingRecord = false;
        isBackuping = false;
    }
    setTimeout(checkSave, 2000);
}

function checkSave() {
    if (waitingRecord)
        setTimeout(waitForSave, 2000);
}

function clearOldBackup()
{
    if(_MAX_SAVE_TIME >= 0)
    {
        _LOG('[Info] 备份最长保存时间：' + _MAX_SAVE_TIME + '天');
        let beforeTime = new Date().getTime() / 1000 - _MAX_SAVE_TIME * 86400;
        runcmd('system ' + _BACKUP_RUNNER + ' "' + _CONFIG_FILE + '" -c "' + String(beforeTime) + '"');
    }
}

function startBackup()
{
    if (isBackuping)
        _LOG('[Error] 已有备份正在执行中，请耐心等待完成。');
    else {
        isBackuping = true;
        clearOldBackup();

        let res = fileReadAllText(_BACKUP_RESULT);
        if (res != null)
            runcmd('system del ' + _BACKUP_RESULT)

        res = runcmd('save hold')
        if (!res)
            _LOG('[Error] 存档备份数据获取失败。备份失败！');
        else {
            waitingRecord = true;
            _LOG('[Info] BDS存档备份开始');
            setTimeout(waitForSave, 1000);
        }
    }
}

function resumeBackup() {
    let res = fileReadAllText(_BACKUP_RESULT);
    if (res == null)
        setTimeout(resumeBackup, 5000);
    else {
        runcmd('save resume');
        if (res == "0")
        {
            _LOG('[Info] 备份成功结束。');
            runcmd('system del ' + _BACKUP_RESULT);
            runcmd('system del ' + _BACKUP_LIST);
        }
        else
            _LOG('[Error] 备份失败！错误码：' + res);
        runcmd('system rd /S /Q ' + _TEMP_PATH)
        isBackuping = false;
    }
}

function autoBackup()
{
    if(!autoBakEnabled)
        return;
    _LOG('【自动备份时间已到】备份启动中...');
    _LOG('注：距下一次自动备份还有'+_AUTO_BAK_INTERVAL+'小时');
    setTimeout(autoBackup,_AUTO_BAK_INTERVAL*3600000);
    startBackup();
}

addBeforeActListener('onServerCmd', function (e) {
    let je = JSON.parse(e);
    let cmd = je.cmd.trim();
    if (cmd == 'backup')
    {
        startBackup();
        return false;
    }
    else if (cmd == 'backup reload')
    {
        _LOG('重新加载配置文件...');
        initLocalConfig();
        return false;
    }
    return true;
});

addBeforeActListener('onServerCmdOutput', function (e) {
    let je = JSON.parse(e);
    let optStr = je.output;
    if (waitingRecord && optStr.indexOf("Data saved.") != -1) {
        waitingRecord = false;
        _LOG('[Info] 已抓取到BDS待备份清单。正在处理...');

        let contexts = optStr.substr(optStr.indexOf('\n') + 1);
        let fLists = contexts.split(', ');

        mkdir(_TEMP_PATH);
        fileWriteAllText(_BACKUP_LIST, '');
        for (let i = 0; i < fLists.length; i++) {
            let datas = fLists[i].split(':');
            let res = fileWriteLine(_BACKUP_LIST, datas[0]);
            if (!res) {
                _LOG('[Error] 待备份清单输出失败。备份失败！');
                isBackuping = false;
                return true;
            }
            if (!buildDirTree(datas[0])) {
                _LOG('[Error] 备份目录结构建立失败。备份失败！');
                isBackuping = false;
                return true;
            }
            res = fileWriteLine(_BACKUP_LIST, datas[1]);
            if (!res) {
                _LOG('[Error] 待备份清单输出失败。备份失败！');
                isBackuping = false;
                return true;
            }
        }
        _LOG('[Info] 处理完毕。清单已输出到文件：BDS目录' + _BACKUP_LIST);
        _LOG('[Info] 启动备份进程...');

        let res = runcmd('system ' + _BACKUP_RUNNER + ' "' + _CONFIG_FILE + '" "' + _BACKUP_LIST + '"');
        if (!res) {
            _LOG('[Error] 备份进程启动失败。备份失败！');
            isBackuping = false;
            return true;
        }
        _LOG('[Info] 完毕。备份进程工作中');

        setTimeout(resumeBackup, 5000);
        return false;
    }
});

var _BACKUP_LIST = '.\\plugins\\BackupHelper\\latest.txt';
var _BACKUP_RUNNER = '.\\plugins\\BackupHelper\\BackupRunner.exe';
var _TEMP_PATH = '.\\plugins\\BackupHelper\\temp';
var _BACKUP_RESULT = '.\\plugins\\BackupHelper\\end.res';

var _MAX_SAVE_TIME = 14;
var _AUTO_BAK_INTERVAL = 0;

// -------------------- Main -------------------- //

_LOG('BackupHelper存档备份助手-已装载  当前版本：' + _VER);
_LOG('后台命令： backup 开始备份');
_LOG('后台命令： backup reload 重新加载配置文件');
_LOG('作者：yqs112358   首发平台：MineBBS');
_LOG('欲联系作者可前往MineBBS论坛');

initLocalConfig();