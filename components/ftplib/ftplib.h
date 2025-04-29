/**
 * @file
 * @brief ESP32-FTP-Client Description
 *
 *
 * @note
 * Copyright � 2019 Evgeniy Ivanov. Contacts: <strelok1290@gmail.com>
 * Copyright � 1996-2001, 2013, 2016 Thomas Pfau, tfpfau@gmail.com
 * All rights reserved.
 * @note
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 * @note
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @note
 * This file is a part of JB_Lib.
 */

#ifndef FTPLIB_H_
#define FTPLIB_H_

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define FTPLIB_DEBUG					0

#define FTPLIB_BUFFER_SIZE 				4096
#define FTPLIB_RESPONSE_BUFFER_SIZE 	1024
#define FTPLIB_TEMP_BUFFER_SIZE 		1024
#define FTPLIB_ACCEPT_TIMEOUT 			30

/* FtpAccess() type codes */
#define FTPLIB_DIR 						1
#define FTPLIB_DIR_VERBOSE 				2
#define FTPLIB_FILE_READ 				3
#define FTPLIB_FILE_WRITE 				4
#define FTPLIB_MLSD 					5

/* FtpAccess() mode codes */
#define FTPLIB_ASCII 					'A'
#define FTPLIB_IMAGE 					'I'
#define FTPLIB_TEXT 					FTPLIB_ASCII
#define FTPLIB_BINARY 					FTPLIB_IMAGE

/* connection modes */
#define FTPLIB_PASSIVE 					1
#define FTPLIB_ACTIVE 					2

/* connection option names */
#define FTPLIB_CONNMODE 				1
#define FTPLIB_CALLBACK 				2
#define FTPLIB_IDLETIME 				3
#define FTPLIB_CALLBACKARG 				4
#define FTPLIB_CALLBACKBYTES 			5

typedef struct NetBuf NetBuf_t;

typedef int (*FtpCallback_t)(NetBuf_t* nControl, uint32_t xfered, void* arg);

typedef struct
{
	FtpCallback_t cbFunc;			/* function to call */
    void* 				cbArg;			/* argument to pass to function */
    unsigned int 		bytesXferred;	/* callback if this number of bytes transferred */
    unsigned int 		idleTime;		/* callback if this many milliseconds have elapsed */
} FtpCallbackOptions_t;

typedef struct
{
	/*Miscellaneous Functions*/
	int (*FtpSite)(const char* cmd, NetBuf_t* nControl);
	char* (*FtpGetLastResponse)(NetBuf_t* nControl);
	int (*FtpGetSysType)(char* buf, int max, NetBuf_t* nControl);
	int (*FtpGetFileSize)(const char* path,
			unsigned int* size, char mode, NetBuf_t* nControl);
	int (*FtpGetModDate)(const char* path, char* dt,
			int max, NetBuf_t* nControl);
	int (*FtpSetCallback)(const FtpCallbackOptions_t* opt, NetBuf_t* nControl);
	int (*FtpClearCallback)(NetBuf_t* nControl);
	/*Server connection*/
	int (*FtpConnect)(const char* host, uint16_t port, NetBuf_t** nControl);
	int (*FtpLogin)(const char* user, const char* pass, NetBuf_t* nControl);
	void (*FtpQuit)(NetBuf_t* nControl);
	int (*FtpSetOptions)(int opt, long val, NetBuf_t* nControl);
	/*Directory Functions*/
	int (*FtpChangeDir)(const char* path, NetBuf_t* nControl);
	int (*FtpMakeDir)(const char* path, NetBuf_t* nControl);
	int (*FtpRemoveDir)(const char* path, NetBuf_t* nControl);
	int (*FtpDir)(const char* outputfile, const char* path, NetBuf_t* nControl);
	int (*FtpNlst)(const char* outputfile, const char* path,
		NetBuf_t* nControl);
	int (*FtpMlsd)(const char* outputfile, const char* path,
		NetBuf_t* nControl);
	int (*FtpChangeDirUp)(NetBuf_t* nControl);
	int (*FtpPwd)(char* path, int max, NetBuf_t* nControl);
	/*File to File Transfer*/
	int (*FtpGet)(const char* outputfile, const char* path,
			char mode, NetBuf_t* nControl);
	int (*FtpPut)(const char* inputfile, const char* path, char mode,
		NetBuf_t* nControl);
	int (*FtpDelete)(const char* fnm, NetBuf_t* nControl);
	int (*FtpRename)(const char* src, const char* dst, NetBuf_t* nControl);
	/*File to Program Transfer*/
	int (*FtpAccess)(const char* path, int typ, int mode, NetBuf_t* nControl,
	    NetBuf_t** nData);
	int (*FtpRead)(void* buf, int max, NetBuf_t* nData);
	int (*FtpWrite)(const void* buf, int len, NetBuf_t* nData);
	int (*FtpClose)(NetBuf_t* nData);
} FtpClient;

FtpClient* getFtpClient(void);

#ifdef __cplusplus
}
#endif



#endif /* FTPLIB_H_ */
