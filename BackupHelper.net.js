// 文件名：BackupHelper.net.js
// 文件功能：NetJs平台下BackupHelper存档备份插件
// 作者：yqs112358
// 首发平台：MineBBS

let _VER = '1.1.2'
let _RECORD_OUTPUT = '.\\plugins\\BackupHelper\\latest.txt'
let _BACKUP_EXE = '.\\plugins\\BackupHelper\\BackupRunner.exe'

let waitingRecord = false;
let isBackuping = false;

function _LOG(e) {
	log('[BackupHelper] ' + e);
}

function initConfig()
{
    mkdir('.\\plugins');
    mkdir('.\\plugins\\BackupHelper');
}

function waitForSave()
{
    let res = runcmd('save query')
    if(!res)
    {
        _LOG('[Error] 存档备份数据获取失败！');
        waitingRecord = false;
        isBackuping = false;
    }
    setTimeout(checkSave, 2000);
}

function checkSave()
{
    if(waitingRecord)
        setTimeout(waitForSave, 2000);
}

function resumeBackup()
{
    let res = fileReadAllText(".\\plugins\\BackupHelper\\end.res");
    if(res == null)
        setTimeout(resumeBackup, 5000);
    else
    {
        runcmd('save resume');
        if(res == "0")
            _LOG('[Info] 备份成功结束。');
        else
            _LOG('[Error] 备份失败！错误码：'+res);
        runcmd('system del .\\plugins\\BackupHelper\\end.res')
        isBackuping = false;
    }
}

function buildDirStruct(dest)
{
    let dirList = dest.split('/');
    let nowPath = '.\\plugins\\BackupHelper\\temp';
    for(let i = 0; i < dirList.length-1; i++)
    {
        nowPath = nowPath + '\\' + dirList[i];
        if(!mkdir(nowPath))
            return false;
    }
    return true;
}

// -------------------- Main -------------------- //

addBeforeActListener('onServerCmd', function (e) {
	let je = JSON.parse(e);
	let cmd = je.cmd.trim();
	if (cmd == "")
		return true;
	let cmdstrs = cmd.split(' ');
	if (cmdstrs.length > 0) {
        if (cmdstrs[0] == 'backup') {	// 进入命令
            if(isBackuping)
                _LOG('[Error] 已有备份正在执行中，请耐心等待完成。');
            else
            {
                isBackuping = true;
                let res = fileReadAllText(".\\plugins\\BackupHelper\\end.res");
                if(res != null)
                    runcmd('system del .\\plugins\\BackupHelper\\end.res')
                
                res = runcmd('save hold')
                if(!res)
                    _LOG('[Error] 存档备份数据获取失败。备份失败！');
                else
                {
                    waitingRecord = true;
                    _LOG('[Info] BDS备份开始');
                    setTimeout(waitForSave, 1000);
                }
                return false;
            }
		}
	}
	return true;
});

addBeforeActListener('onServerCmdOutput', function (e) {
    let je = JSON.parse(e);
    let optStr = je.output;
    if(waitingRecord && optStr.indexOf("Data saved.") != -1)
    {
        waitingRecord = false;
        _LOG('[Info] 已抓取到BDS待备份清单。正在处理...');

        let contexts = optStr.substr(optStr.indexOf('\n')+1);
        let fLists = contexts.split(', ');

        mkdir('.\\plugins\\BackupHelper\\temp');
        fileWriteAllText(_RECORD_OUTPUT, '');
        for (let i = 0; i < fLists.length; i++)
        {
            let datas = fLists[i].split(':');
            let res = fileWriteLine(_RECORD_OUTPUT, datas[0]);
            if(!res)
            {
                _LOG('[Error] 待备份清单输出失败。备份失败！');
                isBackuping = false;
                return true;
            }
            if(!buildDirStruct(datas[0]))
            {
                _LOG('[Error] 备份目录结构建立失败。备份失败！');
                isBackuping = false;
                return true;
            }
            res = fileWriteLine(_RECORD_OUTPUT, datas[1]);
            if(!res)
            {
                _LOG('[Error] 待备份清单输出失败。备份失败！');
                isBackuping = false;
                return true;
            }
        }
        _LOG('[Info] 处理完毕。清单已输出到文件：BDS目录'+_RECORD_OUTPUT);
        _LOG('[Info] 启动备份进程...');

        let res = runcmd('system '+_BACKUP_EXE + ' "' + _RECORD_OUTPUT+'"');    
        if(!res)
        {
            _LOG('[Error] 备份进程启动失败。备份失败！');
            isBackuping = false;
            return true;
        }
        _LOG('[Info] 完毕。备份进程工作中');

        setTimeout(resumeBackup, 5000);
        return false;
    }
});

initConfig();

_LOG('BackupHelper存档备份助手-已装载  当前版本：'+_VER);
_LOG('作者：yqs112358   首发平台：MineBBS');
_LOG('欲联系作者可前往MineBBS论坛');
_LOG('用法：后台输入 backup 命令激活BDS备份系统');