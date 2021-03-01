// 文件名：BackupRunner.c
// 文件功能：NetJs平台下BackupHelper存档备份插件--辅助程序
// 作者：yqs112358
// 首发平台：MineBBS

#define WORLD_PATH ".\\worlds\\"
#define BACKUP_PATH ".\\backup\\"
#define TMP_PATH ".\\plugins\\BackupHelper\\temp\\"
#define ZIP_PATH ".\\plugins\\BackupHelper\\7za.exe"
#define END_RESULT ".\\plugins\\BackupHelper\\end.res"
#define RECORD_FILE argv[1]

#define BUFFER_SIZE 16384
#define GROUP_OF_FILES 10

#define SHOW_COPY_PROCESS_OPEN

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <direct.h>

bool copyFile(const char *fromFile,const char *toFile,int size);
void transStr(char *str);
void clearTemp();
void failExit(int code);

int main(int argc,char *argv[])
{
    setbuf(stdout,NULL);

    if(argc <= 1)
    {
        printf("[BackupProcess][Error] Need more argument!\r\n");
        failExit(errno);
    }

    printf("[BackupProcess] Backup process begin\r\n");

    //Read record file and Copy Save files
    FILE *fp=fopen(RECORD_FILE,"r");
    if(fp == NULL)
    {
        printf("[BackupProcess][Error] Failed to open Record File!\r\n");
        failExit(errno);
    }
    char filePath[_MAX_PATH];
    int fileLength;
    char saveName[_MAX_PATH],*from,*to=saveName;
    int fileCount = 0;
    while(fgets(filePath,_MAX_PATH,fp) != NULL)
    {
        //Read
        if(filePath[0] == '\r' || filePath[0] == '\n')
            break;
        ++fileCount;
        transStr(filePath);

        fscanf(fp,"%d",&fileLength);
        fgetc(fp);      //Skip \n

        //Get save name
        if(fileCount == 1)
        {
            from=filePath;
            while(*from != '\\')
                *to++ = *from++;
            *to='\0';
        }
        //Copy
        char fromFile[_MAX_PATH]=WORLD_PATH;
        strcat(fromFile,filePath);
        char toFile[_MAX_PATH]=TMP_PATH;
        strcat(toFile,filePath);
        //printf("[BackupProcess] Processing %s\r\n",filePath);
        if(!copyFile(fromFile,toFile,fileLength))
        {
            printf("[BackupProcess][Error] Failed to copy %s!\r\n",filePath);
            failExit(errno);
        }
#ifdef SHOW_COPY_PROCESS_OPEN
        if(fileCount % GROUP_OF_FILES == 0)
            printf("[BackupProcess] %d files processed.\r\n",fileCount);
#endif
    }
    fclose(fp);
    printf("[BackupProcess] All of %d files processed.\r\n",fileCount);

    

    //Zip and clear
    printf("[BackupProcess] Files copied. Zipping save files...\r\n");
    printf("[BackupProcess] It may take a long time. Please be patient...\r\n");
    char timeStr[32];
    time_t nowtime;
    time(&nowtime);
    struct tm *info = localtime(&nowtime);
    strftime(timeStr,sizeof(timeStr),"%Y-%m-%d-%H-%M-%S", info);

    char cmdStr[_MAX_PATH*4];
    sprintf(cmdStr,"%s a \"%s%s-%s.7z\" \"%s%s\" -sdel -mx1 -mmt >nul",ZIP_PATH,BACKUP_PATH,saveName,timeStr,TMP_PATH,saveName);

    int res = system(cmdStr);
    if(res != 0)
    {
        printf("[BackupProcess][Error] Failed to Zip save files!\r\n");
        failExit(res);
    }

    printf("[BackupProcess][Info] Success to Backup.\r\n");
    //Flag to success
    fp=fopen(END_RESULT,"w");
    fputs("0",fp);
    fclose(fp);
    return 0;
}

/////////////////////// Main End ///////////////////////
////////////////////////////////////////////////////////


bool copyFile(const char *fromFile,const char *toFile,int size)
{
    ///////////// DEBUG
    //printf("        From \"%s\"\r\n        To \"%s\"\r\n",fromFile,toFile);
    //char buf[_MAX_PATH];
    //getcwd(buf, _MAX_PATH);
    //printf("        Working path:%s\r\n",buf);
    ////////////
    char buffer[BUFFER_SIZE];

    FILE *fFrom=fopen(fromFile,"rb");
    FILE *fTo=fopen(toFile,"wb");
    if(fFrom == NULL)
    {
        printf("[BackupProcess][Error] Failed to open Source file!\r\n");
        return false;
    }
    if(fTo == NULL)
    {
        printf("[BackupProcess][Error] Failed to open Destination file!\r\n");
        return false;
    }
    
    int readCount;
    while(size > 0)
    {
        readCount=fread(buffer,sizeof(char), (size>BUFFER_SIZE)?BUFFER_SIZE:size, fFrom);
        if(readCount == 0 && !feof(fFrom))
        {
            printf("[BackupProcess][Error] Failed to read Source file!\r\n");
            return false;
        }
        fwrite(buffer,sizeof(char),readCount,fTo);
        size-=readCount;
    }

    fclose(fFrom);
    fclose(fTo);
    return true;
}

void transStr(char *str)
{
    do
    {
        if(*str == '/')
            *str = '\\';
        if(*str == '\r' || *str == '\n')
        {
            *str = '\0';
            break;
        }
    }
    while(*str++ != '\0');
}

void clearTemp()
{
    char clearCmd[_MAX_PATH+10]="rd /s /q ";
    strcat(clearCmd,TMP_PATH);
    system(clearCmd);
}

void failExit(int code)
{
    clearTemp();
    printf("[BackupProcess][FATAL] Backup failed. Error Code: %d",code);
    //Flag to fail
    FILE *fp=fopen(END_RESULT,"w");
    fprintf(fp,"%d",code);
    fclose(fp);
    exit(code);
}
