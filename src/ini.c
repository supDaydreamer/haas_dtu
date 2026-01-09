#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <pthread.h>

char *GetIniKeyString(char *title,char *key,char *filename)  
{  
	FILE *fp;  
	int  flag = 0;  
	char sTitle[32], *wTmp;  
	static char sLine[1024];  
  
	sprintf(sTitle, "[%s]", title);  
	if(NULL == (fp = fopen(filename, "r"))) {  
		perror("fopen");  
		return "0";  
	}  
  
	while (NULL != fgets(sLine, 1024, fp)) {  
		// 这是注释行  
		if (0 == strncmp("//", sLine, 2)) continue;  
		if ('#' == sLine[0])			  continue;  
  
		wTmp = strchr(sLine, '=');  
		if ((NULL != wTmp) && (1 == flag)) {  
			if (0 == strncmp(key, sLine, wTmp-sLine)) { // 长度依文件读取的为准  
				sLine[strlen(sLine) - 1] = '\0';  
				fclose(fp);  
				return wTmp + 1;  
			}  
		} else {  
		//	printf("sTitle:%s,sLine:%s\r\n",sTitle,sLine);
			if (0 == strncmp(sTitle, sLine, strlen(sLine) - 1)) { // 长度依文件读取的为准  
				flag = 1; // 找到标题位置 
		//		printf("match ok!!!\r\n");
			}  
		}  
	}  
	fclose(fp);  
	return "0";  
}

int GetIniKeyInt(char *title,char *key,char *filename)  
{  
	return atoi(GetIniKeyString(title, key, filename));  
}  

pthread_mutex_t ini_mutex = PTHREAD_MUTEX_INITIALIZER;
char sBuf[1024 * 100] = {0};
char *sBufp = sBuf;
char sLine[1024] = {0};
char sTitle[128] = {0};

int PutIniKeyString(char *title, char *key, char *val, char *filename)  
{
	//printf("[ini] set %s=%s in [%s] on %s\n", key, val, title, filename);
	//fflush(stdout);

	//printf("[ini] try pthread mute...\n");
	//fflush(stdout);
	pthread_mutex_lock(&ini_mutex);

	if (0 != access(filename, F_OK)) {
		printf("[ini] error: file <%s> not exist!\n", filename);
		fflush(stdout);
		return -1;
	}

	FILE *fpr = NULL;
	int fdr = -1;
	int flag = 0;
	char *wTmp = NULL;

	sprintf(sTitle, "[%s]", title);

	if (NULL == (fpr = fopen(filename, "r+"))) {
		printf("[ini] error: open file <%s> error!\n", filename);
		fflush(stdout);
		return -1;
	}

	fdr = fileno(fpr);
	//printf("[ini] try flock...\n");
	//fflush(stdout);
	if (0 != flock(fdr, LOCK_EX)) {
		printf("[ini] error: can't get file <%s> lock\n", filename);
		fflush(stdout);
		return -1;
	}
  
#if JUST_FOR_TEST
	int i;
	for (i = 0; i < 100; i++) {
		printf("--- <%s> ---> [%d]\n", val, i + 1);
		fflush(stdout);
		usleep(100 * 1000);
	}
#endif

	sBufp = sBuf;
	while (NULL != fgets(sLine, 1024, fpr)) {  
		if (2 != flag) { // 如果找到要修改的那一行，则不会执行内部的操作  
			wTmp = strchr(sLine, '=');  
			if ((NULL != wTmp) && (1 == flag)) {  
				if (0 == strncmp(key, sLine, wTmp-sLine)) { // 长度依文件读取的为准  
					flag = 2;// 更改值，方便写入文件  
					sprintf(wTmp + 1, "%s\n", val);  
				}  
			} else {  
				if (0 == strncmp(sTitle, sLine, strlen(sLine) - 1)) { // 长度依文件读取的为准  
					flag = 1; // 找到标题位置  
				}  
			}  
		}  
  
		if (sBufp + strlen(sLine) + 1 > sBuf + sizeof(sBuf)) {
			printf("[ini] error: file <%s> out of write buffer!\n", filename);
			fflush(stdout);
			return -1;
		}
		memcpy(sBufp, sLine, strlen(sLine)); // 写入临时文件  
		sBufp[strlen(sLine)] = '\0';
		sBufp += strlen(sLine);
	}
	//printf("------ file <%s> write ------\n%s\n========================\n", filename, sBuf);
	//fflush(stdout);
	ftruncate(fdr, 0);
	fseek(fpr, 0, SEEK_SET);
	fputs(sBuf, fpr);
	flock(fdr, LOCK_UN);
	fclose(fpr);  
  
	pthread_mutex_unlock(&ini_mutex);
	return 0;
}

int PutIniKeyInt(char *title,char *key,int val,char *filename)  
{  
	char sVal[32];  
	sprintf(sVal, "%d", val);  
	return PutIniKeyString(title, key, sVal, filename);  
} 

