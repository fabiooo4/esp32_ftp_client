/**
 * @file
 * @brief ESP32-FTP-Client Realization
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
 *	   http://www.apache.org/licenses/LICENSE-2.0
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

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include "ftplib.h"

#include "netdb.h"

#include "esp_log.h"

#if !defined FTPLIB_DEFAULT_MODE
#define FTPLIB_DEFAULT_MODE			FTPLIB_PASSIVE
#endif

#define FTPLIB_CONTROL					0
#define FTPLIB_READ						1
#define FTPLIB_WRITE					2

struct NetBuf {
	char* cput;
	char* cget;
	int handle;
	int cavail, cleft;
	char* buf;
	int dir;
	NetBuf_t* ctrl;
	NetBuf_t* data;
	int cmode;
	struct timeval idletime;
	FtpCallback_t idlecb;
	void* idlearg;
	unsigned long int xfered;
	unsigned long int cbbytes;
	unsigned long int xfered1;
	char response[FTPLIB_RESPONSE_BUFFER_SIZE];
};

/*Internal use functions*/
static int socketWait(NetBuf_t* ctl);
static int readResponse(char c, NetBuf_t* nControl);
static int readLine(char* buffer, int max, NetBuf_t* ctl);
static int sendCommand(const char* cmd, char expresp, NetBuf_t* nControl);
static int xfer(const char* localfile, const char* path,
	NetBuf_t* nControl, int typ, int mode);
static int openPort(NetBuf_t* nControl, NetBuf_t** nData, int mode, int dir);
static int writeLine(const char* buf, int len, NetBuf_t* nData);
static int acceptConnection(NetBuf_t* nData, NetBuf_t* nControl);

/*
 * socket_wait - wait for socket to receive or flush data
 *
 * return 1 if no user callback, otherwise, return value returned by
 * user callback
 */
static int socketWait(NetBuf_t* ctl)
{
	fd_set fd;
	fd_set* rfd = NULL;
	fd_set* wfd = NULL;
	struct timeval tv;
	int rv = 0;

	if ((ctl->dir == FTPLIB_CONTROL) || (ctl->idlecb == NULL))
		return 1;
	if (ctl->dir == FTPLIB_WRITE)
		wfd = &fd;
	else
		rfd = &fd;
	FD_ZERO(&fd);
	do
	{
		FD_SET(ctl->handle, &fd);
		tv = ctl->idletime;
		rv = select((ctl->handle + 1), rfd, wfd, NULL, &tv);
		if (rv == -1) {
			rv = 0;
			strncpy(ctl->ctrl->response, strerror(errno),
						sizeof(ctl->ctrl->response));
			break;
		}
		else if (rv > 0) {
			rv = 1;
			break;
		}
	}
	while ((rv = ctl->idlecb(ctl, ctl->xfered, ctl->idlearg)));
	return rv;
}



/*
 * read a line of text
 *
 * return -1 on error or bytecount
 */
static int readLine(char* buffer, int max, NetBuf_t* ctl)
{
	if ((ctl->dir != FTPLIB_CONTROL) && (ctl->dir != FTPLIB_READ))
		return -1;
	if (max == 0)
		return 0;

	int x,retval = 0;
	char *end,*bp = buffer;
	int eof = 0;
	while (1) {
		if (ctl->cavail > 0) {
			x = (max >= ctl->cavail) ? ctl->cavail : (max-1);
			end = memccpy(bp, ctl->cget, '\n',x);
			if (end != NULL)
				x = end - bp;
			retval += x;
			bp += x;
			*bp = '\0';
			max -= x;
			ctl->cget += x;
			ctl->cavail -= x;
			if (end != NULL)
			{
				bp -= 2;
				if (strcmp(bp,"\r\n") == 0) {
					*bp++ = '\n';
					*bp++ = '\0';
					--retval;
				}
				break;
			}
		}
		if (max == 1) {
			*buffer = '\0';
			break;
		}
		if (ctl->cput == ctl->cget) {
			ctl->cput = ctl->cget = ctl->buf;
			ctl->cavail = 0;
			ctl->cleft = FTPLIB_BUFFER_SIZE;
		}
		if (eof) {
			if (retval == 0)
				retval = -1;
			break;
		}
		if (!socketWait(ctl))
			return retval;
		if ((x = recv(ctl->handle, ctl->cput,ctl->cleft, 0)) == -1) {
			#if FTPLIB_DEBUG
			perror("FTP Client Error: realLine, read");
			#endif
			retval = -1;
			break;
		}
		if (x == 0)
			eof = 1;
		ctl->cleft -= x;
		ctl->cavail += x;
		ctl->cput += x;
	}
	return retval;
}



/*
 * read a response from the server
 *
 * return 0 if first char doesn't match
 * return 1 if first char matches
 */
static int readResponse(char c, NetBuf_t* nControl)
{
	char match[5];
	if (readLine(nControl->response,
			FTPLIB_RESPONSE_BUFFER_SIZE, nControl) == -1) {
		#if FTPLIB_DEBUG
		perror("FTP Client Error: readResponse, read failed");
		#endif
		return 0;
	}
	#if FTPLIB_DEBUG == 2
	printf("FTP Client Response: %s\n\r", nControl->response);
	#endif
	if (nControl->response[3] == '-')
	{
		strncpy(match, nControl->response, 3);
		match[3] = ' ';
		match[4] = '\0';
		do {
			if (readLine(nControl->response,
					FTPLIB_RESPONSE_BUFFER_SIZE, nControl) == -1) {
				#if FTPLIB_DEBUG
				perror("FTP Client Error: readResponse, read failed");
				#endif
				return 0;
			}
			#if FTPLIB_DEBUG == 2
			printf("FTP Client Response: %s\n\r", nControl->response);
			#endif
		}
		while (strncmp(nControl->response, match, 4));
	}
	if(nControl->response[0] == c)
		return 1;
	else
		return 0;
}



/*
 * sendCommand - send a command and wait for expected response
 *
 * return 1 if proper response received, 0 otherwise
 */
static int sendCommand(const char* cmd, char expresp, NetBuf_t* nControl)
{
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	if (nControl->dir != FTPLIB_CONTROL)
		return 0;
	#if FTPLIB_DEBUG == 2
	printf("FTP Client sendCommand: %s\n\r", cmd);
	#endif
	if ((strlen(cmd) + 3) > sizeof(buf))
		return 0;
	sprintf(buf, "%s\r\n", cmd);
	if (send(nControl->handle, buf, strlen(buf), 0) <= 0) {
		#if FTPLIB_DEBUG
		perror("FTP Client sendCommand: write");
		#endif
		return 0;
	}
	return readResponse(expresp, nControl);
}



/*
 * Xfer - issue a command and transfer data
 *
 * return 1 if successful, 0 otherwise
 */
static int xfer(const char* localfile, const char* path,
	NetBuf_t* nControl, int typ, int mode)
{
	FILE* local = NULL;
	NetBuf_t* nData;

	if (localfile != NULL) {
		char ac[4];
		memset( ac, 0, sizeof(ac) );
		if (typ == FTPLIB_FILE_WRITE)
			ac[0] = 'r';
		else
			ac[0] = 'w';
		if (mode == FTPLIB_IMAGE)
			ac[1] = 'b';
		local = fopen(localfile, ac);
		if (local == NULL) {
			strncpy(nControl->response, strerror(errno),
						sizeof(nControl->response));
			return 0;
		}
	}
	if(local == NULL)
		local = (typ == FTPLIB_FILE_WRITE) ? stdin : stdout;
	if (!FtpAccess(path, typ, mode, nControl, &nData)) {
		if (localfile) {
			fclose(local);
			if (typ == FTPLIB_FILE_READ)
				unlink(localfile);
		}
		return 0;
	}

	int rv = 1;
	int l = 0;
	char* dbuf = malloc(FTPLIB_BUFFER_SIZE);
	if (dbuf != NULL) {
		if (typ == FTPLIB_FILE_WRITE) {
			while ((l = fread(dbuf, 1, FTPLIB_BUFFER_SIZE, local)) > 0) {
				int c = FtpWrite(dbuf, l, nData);
				if (c < l) {
					#if FTPLIB_DEBUG
					//printf("Ftp Client xfer short write: passed %d, wrote %d\n", l, c);
					char tempbuf[128];
					sprintf(tempbuf, "Ftp Client xfer short write: passed %d, wrote %d\n", l, c);
					perror(tempbuf);
					#endif
					rv = 0;
					break;
				}
			}
		}
		else {
			while ((l = FtpRead(dbuf, FTPLIB_BUFFER_SIZE, nData)) > 0) {
				if (fwrite(dbuf, 1, l, local) == 0) {
					#if FTPLIB_DEBUG
					perror("FTP Client xfer localfile write");
					#endif
					rv = 0;
					break;
				}
			}
		}
		free(dbuf);
	} else {
		#if FTPLIB_DEBUG
		perror("FTP Client xfer malloc dbuf");
		#endif
		rv = 0;
	}
	fflush(local);
	if(localfile != NULL){
		fclose(local);
		if(rv != 1)
			unlink(localfile);
	}
	FtpClose(nData);
	return rv;
}



/*
 * openPort - set up data connection
 *
 * return 1 if successful, 0 otherwise
 */
static int openPort(NetBuf_t* nControl, NetBuf_t** nData, int mode, int dir)
{
	union
	{
		struct sockaddr sa;
		struct sockaddr_in in;
	} sin;

	if (nControl->dir != FTPLIB_CONTROL)
		return -1;
	if ((dir != FTPLIB_READ) && (dir != FTPLIB_WRITE)) {
		sprintf(nControl->response, "Invalid direction %d\n", dir);
		return -1;
	}
	if ((mode != FTPLIB_ASCII) && (mode != FTPLIB_IMAGE)) {
		sprintf(nControl->response, "Invalid mode %c\n", mode);
		return -1;
	}
	//unsigned int l = sizeof(sin);
	socklen_t l = sizeof(sin);
	if (nControl->cmode == FTPLIB_PASSIVE) {
		memset(&sin, 0, l);
		sin.in.sin_family = AF_INET;
		if (!sendCommand("PASV", '2', nControl))
			return -1;
		char* cp = strchr(nControl->response,'(');
		if (cp == NULL)
			return -1;
		cp++;
		unsigned int v[6];
		sscanf(cp,"%u,%u,%u,%u,%u,%u",&v[2],&v[3],&v[4],&v[5],&v[0],&v[1]);
		sin.sa.sa_data[2] = v[2];
		sin.sa.sa_data[3] = v[3];
		sin.sa.sa_data[4] = v[4];
		sin.sa.sa_data[5] = v[5];
		sin.sa.sa_data[0] = v[0];
		sin.sa.sa_data[1] = v[1];
	}
	else {
		if(getsockname(nControl->handle, &sin.sa, &l) < 0) {
			#if FTPLIB_DEBUG
			perror("FTP Client openPort: getsockname");
			#endif
			return -1;
		}
	}
	int sData = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sData == -1) {
		#if FTPLIB_DEBUG
		perror("FTP Client openPort: socket");
		#endif
		return -1;
	}
	if (nControl->cmode == FTPLIB_PASSIVE) {
		if (connect(sData, &sin.sa, sizeof(sin.sa)) == -1) {
			#if FTPLIB_DEBUG
			perror("FTP Client openPort: connect");
			#endif
			closesocket(sData);
			return -1;
		}
	}
	else {
		sin.in.sin_port = 0;
		if (bind(sData, &sin.sa, sizeof(sin)) == -1) {
			#if FTPLIB_DEBUG
			perror("FTP Client openPort: bind");
			#endif
			closesocket(sData);
			return -1;
		}
		if (listen(sData, 1) < 0) {
			#if FTPLIB_DEBUG
			perror("FTP Client openPort: listen");
			#endif
			closesocket(sData);
			return -1;
		}
		if (getsockname(sData, &sin.sa, &l) < 0)
			return -1;
		char buf[FTPLIB_TEMP_BUFFER_SIZE];
		sprintf(buf, "PORT %d,%d,%d,%d,%d,%d",
			(unsigned char) sin.sa.sa_data[2],
			(unsigned char) sin.sa.sa_data[3],
			(unsigned char) sin.sa.sa_data[4],
			(unsigned char) sin.sa.sa_data[5],
			(unsigned char) sin.sa.sa_data[0],
			(unsigned char) sin.sa.sa_data[1]);
		if (!sendCommand(buf, '2', nControl)) {
			closesocket(sData);
			return -1;
		}
	}
	NetBuf_t* ctrl = calloc(1, sizeof(NetBuf_t));
	if (ctrl == NULL) {
		#if FTPLIB_DEBUG
		perror("FTP Client openPort: calloc ctrl");
		#endif
		closesocket(sData);
		return -1;
	}
	if ((mode == 'A') && ((ctrl->buf = malloc(FTPLIB_BUFFER_SIZE)) == NULL)) {
		#if FTPLIB_DEBUG
		perror("FTP Client openPort: malloc ctrl->buf");
		#endif
		closesocket(sData);
		free(ctrl);
		return -1;
	}
	ctrl->handle = sData;
	ctrl->dir = dir;
	ctrl->idletime = nControl->idletime;
	ctrl->idlearg = nControl->idlearg;
	ctrl->xfered = 0;
	ctrl->xfered1 = 0;
	ctrl->cbbytes = nControl->cbbytes;
	ctrl->ctrl = nControl;
	if (ctrl->idletime.tv_sec || ctrl->idletime.tv_usec || ctrl->cbbytes)
		ctrl->idlecb = nControl->idlecb;
	else
		ctrl->idlecb = NULL;
	nControl->data = ctrl;
	*nData = ctrl;
	return 1;
}



/*
 * write lines of text
 *
 * return -1 on error or bytecount
 */
static int writeLine(const char* buf, int len, NetBuf_t* nData)
{
	if (nData->dir != FTPLIB_WRITE)
		return -1;
	char* nbp = nData->buf;
	int nb = 0;
	int w = 0;
	const char* ubp = buf;
	char lc = 0;
	int x = 0;

	for (x = 0; x < len; x++) {
		if ((*ubp == '\n') && (lc != '\r')) {
			if (nb == FTPLIB_BUFFER_SIZE) {
				if (!socketWait(nData))
					return x;
				w = send(nData->handle, nbp, FTPLIB_BUFFER_SIZE, 0);
				if (w != FTPLIB_BUFFER_SIZE) {
					#if FTPLIB_DEBUG
					printf("Ftp client write line: net_write(1) returned %d, errno = %d\n",
							w, errno);
					#endif
					return(-1);
				}
				nb = 0;
			}
			nbp[nb++] = '\r';
		}
		if (nb == FTPLIB_BUFFER_SIZE) {
			if (!socketWait(nData))
				return x;
			w = send(nData->handle, nbp, FTPLIB_BUFFER_SIZE, 0);
			if (w != FTPLIB_BUFFER_SIZE) {
				#if FTPLIB_DEBUG
				printf("Ftp client write line: net_write(2) returned %d, errno = %d\n",
						w, errno);
				#endif
				return(-1);
			}
			nb = 0;
		}
		nbp[nb++] = lc = *ubp++;
	}
	if (nb){
		if (!socketWait(nData))
			return x;
		w = send(nData->handle, nbp, nb, 0);
		if (w != nb) {
			#if FTPLIB_DEBUG
			printf("Ftp client write line: net_write(3) returned %d, errno = %d\n",
					w, errno);
			#endif
			return(-1);
		}
	}
	return len;
}



/*
 * acceptConnection - accept connection from server
 *
 * return 1 if successful, 0 otherwise
 */
static int acceptConnection(NetBuf_t* nData, NetBuf_t* nControl)
{
	int rv = 0;
	fd_set mask;
	FD_ZERO(&mask);
	FD_SET(nControl->handle, &mask);
	FD_SET(nData->handle, &mask);
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = FTPLIB_ACCEPT_TIMEOUT ;
	int i = nControl->handle;
	if (i < nData->handle)
		i = nData->handle;
	i = select(i+1, &mask, NULL, NULL, &tv);
	if (i == -1) {
		strncpy(nControl->response, strerror(errno),
				sizeof(nControl->response));
		closesocket(nData->handle);
		nData->handle = 0;
		rv = 0;
	}
	else if (i == 0) {
		strcpy(nControl->response, "FTP Client accept connection "
				"timed out waiting for connection");
		closesocket(nData->handle);
		nData->handle = 0;
		rv = 0;
	}
	else {
		if (FD_ISSET(nData->handle, &mask)) {
			struct sockaddr addr;
			//unsigned int l = sizeof(addr);
			socklen_t l = sizeof(addr);
			int sData = accept(nData->handle, &addr, &l);
			i = errno;
			closesocket(nData->handle);
			if (sData > 0) {
				rv = 1;
				nData->handle = sData;
			}
			else {
				strncpy(nControl->response, strerror(i),
								sizeof(nControl->response));
				nData->handle = 0;
				rv = 0;
			}
		}
		else if (FD_ISSET(nControl->handle, &mask)) {
			closesocket(nData->handle);
			nData->handle = 0;
			readResponse('2', nControl);
			rv = 0;
		}
	}
	return rv;
}



/*
 * FtpSite - send a SITE command
 *
 * return 1 if command successful, 0 otherwise
 */
int FtpSite(const char* cmd, NetBuf_t* nControl)
{
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(cmd) + 7) > sizeof(buf))
		return 0;
	sprintf(buf, "SITE %s", cmd);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * FtpGetLastResponse - return a pointer to the last response received
 */
char* FtpGetLastResponse(NetBuf_t* nControl)
{
	if ((nControl) && (nControl->dir == FTPLIB_CONTROL))
		return nControl->response;
	else
		return NULL;
}



/*
 * FtpGetSysType - send a SYST command
 *
 * Fills in the user buffer with the remote system type.  If more
 * information from the response is required, the user can parse
 * it out of the response buffer returned by FtpLastResponse().
 *
 * return 1 if command successful, 0 otherwise
 */
int FtpGetSysType(char* buf, int max, NetBuf_t* nControl)
{
	if (!sendCommand("SYST", '2', nControl))
		return 0;
	char* s = &nControl->response[4];
	int l = max;
	char* b = buf;
	while ((--l) && (*s != ' '))
		*b++ = *s++;
	*b++ = '\0';
	return 1;
}



/*
 * FtpGetFileSize - determine the size of a remote file
 *
 * return 1 if successful, 0 otherwise
 */
int FtpGetFileSize(const char* path,
		unsigned int* size, char mode, NetBuf_t* nControl)
{
	char cmd[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 7) > sizeof(cmd))
		return 0;
	sprintf(cmd, "TYPE %c", mode);
	if (!sendCommand(cmd, '2', nControl))
		return 0;
	int rv = 1;
	sprintf(cmd,"SIZE %s", path);
	if(!sendCommand(cmd, '2', nControl))
		rv = 0;
	else {
		int resp;
		unsigned int sz;
		if (sscanf(nControl->response, "%d %u", &resp, &sz) == 2)
			*size = sz;
		else
			rv = 0;
	}
	return rv;
}



/*
 * FtpGetModDate - determine the modification date of a remote file
 *
 * return 1 if successful, 0 otherwise
 */
int FtpGetModDate(const char* path, char* dt,
		int max, NetBuf_t* nControl)
{
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 7) > sizeof(buf))
		return 0;
	sprintf(buf, "MDTM %s", path);
	int rv = 1;
	if (!sendCommand(buf, '2', nControl))
		rv = 0;
	else
		strncpy(dt, &nControl->response[4], max);
	return rv;
}



int FtpSetCallback(const FtpCallbackOptions_t* opt, NetBuf_t* nControl)
{
   nControl->idlecb = opt->cbFunc;
   nControl->idlearg = opt->cbArg;
   nControl->idletime.tv_sec = opt->idleTime / 1000;
   nControl->idletime.tv_usec = (opt->idleTime % 1000) * 1000;
   nControl->cbbytes = opt->bytesXferred;
   return 1;
}



int FtpClearCallback(NetBuf_t* nControl)
{
   nControl->idlecb = NULL;
   nControl->idlearg = NULL;
   nControl->idletime.tv_sec = 0;
   nControl->idletime.tv_usec = 0;
   nControl->cbbytes = 0;
   return 1;
}



/*
 * FtpConnect - connect to remote server
 *
 * return 1 if connected, 0 if not
 */
int FtpConnect(const char* host, uint16_t port, NetBuf_t** nControl)
{
	ESP_LOGD(__FUNCTION__, "host=%s", host);
	struct sockaddr_in sin;
	memset(&sin,0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(host);
	ESP_LOGD(__FUNCTION__, "sin.sin_addr.s_addr=%"PRIx32, sin.sin_addr.s_addr);
	if (sin.sin_addr.s_addr == 0xffffffff) {
		struct hostent *hp;
		hp = gethostbyname(host);
		if (hp == NULL) {
			#if FTPLIB_DEBUG
			perror("FTP Client Error: Connect, gethostbyname");
			#endif
			return 0;
		}
		struct ip4_addr *ip4_addr;
		ip4_addr = (struct ip4_addr *)hp->h_addr;
		sin.sin_addr.s_addr = ip4_addr->addr;
		ESP_LOGD(__FUNCTION__, "sin.sin_addr.s_addr=%"PRIx32, sin.sin_addr.s_addr);
	}

	int sControl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	ESP_LOGD(__FUNCTION__, "sControl=%d", sControl);
	if (sControl == -1) {
		#if FTPLIB_DEBUG
		perror("FTP Client Error: Connect, socket");
		#endif
		return 0;
	}
	if(connect(sControl, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		#if FTPLIB_DEBUG
		perror("FTP Client Error: Connect, connect");
		#endif
		closesocket(sControl);
		return 0;
	}
	NetBuf_t* ctrl = calloc(1, sizeof(NetBuf_t));
	if (ctrl == NULL) {
		#if FTPLIB_DEBUG
		perror("FTP Client Error: Connect, calloc ctrl");
		#endif
		closesocket(sControl);
		return 0;
	}
	ctrl->buf = malloc(FTPLIB_BUFFER_SIZE);
	if (ctrl->buf == NULL) {
		#if FTPLIB_DEBUG
		perror("FTP Client Error: Connect, malloc ctrl->buf");
		#endif
		closesocket(sControl);
		free(ctrl);
		return 0;
	}
	ctrl->handle = sControl;
	ctrl->dir = FTPLIB_CONTROL;
	ctrl->ctrl = NULL;
	ctrl->data = NULL;
	ctrl->cmode = FTPLIB_DEFAULT_MODE;
	ctrl->idlecb = NULL;
	ctrl->idletime.tv_sec = ctrl->idletime.tv_usec = 0;
	ctrl->idlearg = NULL;
	ctrl->xfered = 0;
	ctrl->xfered1 = 0;
	ctrl->cbbytes = 0;
	if (readResponse('2', ctrl) == 0) {
		closesocket(sControl);
		free(ctrl->buf);
		free(ctrl);
		return 0;
	}
	*nControl = ctrl;
	return 1;
}

/*
 * FtpLogin - log in to remote server
 *
 * return 1 if logged in, 0 otherwise
 */
int FtpLogin(const char* user, const char* pass, NetBuf_t* nControl)
{
	char tempbuf[64];
	if (((strlen(user) + 7) > sizeof(tempbuf)) ||
			((strlen(pass) + 7) > sizeof(tempbuf)))
		return 0;
	sprintf(tempbuf,"USER %s",user);
	if (!sendCommand(tempbuf, '3', nControl)) {
		if (nControl->response[0] == '2')
			return 1;
		return 0;
	}
	sprintf(tempbuf, "PASS %s", pass);
	return sendCommand(tempbuf, '2', nControl);
}



/*
 * FtpQuit - disconnect from remote
 *
 * return 1 if successful, 0 otherwise
 */
void FtpQuit(NetBuf_t* nControl)
{
	if (nControl->dir != FTPLIB_CONTROL)
		return;
	sendCommand("QUIT", '2', nControl);
	closesocket(nControl->handle);
	free(nControl->buf);
	free(nControl);
}



/*
 * FtpSetOptions - change connection options
 *
 * returns 1 if successful, 0 on error
 */
int FtpSetOptions(int opt, long val, NetBuf_t* nControl)
{
	int v,rv = 0;
	switch (opt)
	{
		case FTPLIB_CONNMODE:
		{
			v = (int) val;
			if ((v == FTPLIB_PASSIVE)
					|| (v == FTPLIB_ACTIVE)) {
				nControl->cmode = v;
				rv = 1;
			}
		}
		break;

		case FTPLIB_CALLBACK:
		{
			nControl->idlecb = (FtpCallback_t) val;
			rv = 1;
		}
		break;

		case FTPLIB_IDLETIME:
		{
			v = (int) val;
			rv = 1;
			nControl->idletime.tv_sec = v / 1000;
			nControl->idletime.tv_usec = (v % 1000) * 1000;
		}
		break;

		case FTPLIB_CALLBACKARG:
		{
			rv = 1;
			nControl->idlearg = (void *) val;
		}
		break;

		case FTPLIB_CALLBACKBYTES:
		{
			rv = 1;
			nControl->cbbytes = (int) val;
		}
		break;
	}
	return rv;
}



/*
 * FtpChangeDir - change path at remote
 *
 * return 1 if successful, 0 otherwise
 */
int FtpChangeDir(const char* path, NetBuf_t* nControl)
{
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 6) > sizeof(buf))
		return 0;
	sprintf(buf, "CWD %s", path);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * FtpMakeDir - create a directory at server
 *
 * return 1 if successful, 0 otherwise
 */
int FtpMakeDir(const char* path, NetBuf_t* nControl)
{
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 6) > sizeof(buf))
		return 0;
	sprintf(buf, "MKD %s", path);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * FtpRemoveDir - remove directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int FtpRemoveDir(const char* path, NetBuf_t* nControl)
{
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 6) > sizeof(buf))
		return 0;
	sprintf(buf, "RMD %s", path);
	if (!sendCommand(buf,'2',nControl))
		return 0;
	else
		return 1;
}



/*
 * FtpDir - issue a LIST command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
int FtpDir(const char* outputfile, const char* path, NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTPLIB_DIR_VERBOSE, FTPLIB_ASCII);
}



/*
 * FtpNlst - issue an NLST command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
int FtpNlst(const char* outputfile, const char* path,
	NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTPLIB_DIR, FTPLIB_ASCII);
}



/*
 * FtpNlsd - issue an MLSD command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
int FtpMlsd(const char* outputfile, const char* path,
	NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTPLIB_MLSD, FTPLIB_ASCII);
}



/*
 * FtpChangeDirUp - move to parent directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int FtpChangeDirUp(NetBuf_t* nControl)
{
	if (!sendCommand("CDUP", '2', nControl))
		return 0;
	else
		return 1;
}


/*
 * FtpPwd - get working directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int FtpPwd(char* path, int max, NetBuf_t* nControl)
{
	if (!sendCommand("PWD",'2',nControl))
		return 0;
	char* s = strchr(nControl->response, '"');
	if (s == NULL)
		return 0;
	s++;
	int l = max;
	char* b = path;
	while ((--l) && (*s) && (*s != '"'))
		*b++ = *s++;
	*b++ = '\0';
	return 1;
}



/*
 * FtpGet - issue a GET command and write received data to output
 *
 * return 1 if successful, 0 otherwise
 */
int FtpGet(const char* outputfile, const char* path,
		char mode, NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTPLIB_FILE_READ, mode);
}



/*
 * FtpPut - issue a PUT command and send data from input
 *
 * return 1 if successful, 0 otherwise
 */
int FtpPut(const char* inputfile, const char* path, char mode,
	NetBuf_t* nControl)
{
	return xfer(inputfile, path, nControl, FTPLIB_FILE_WRITE, mode);
}



/*
 * FtpDelete - delete a file at remote
 *
 * return 1 if successful, 0 otherwise
 */
int FtpDelete(const char* fnm, NetBuf_t* nControl)
{
	char cmd[FTPLIB_TEMP_BUFFER_SIZE];
	if ((strlen(fnm) + 7) > sizeof(cmd))
		return 0;
	sprintf(cmd, "DELE %s", fnm);
	if(!sendCommand(cmd, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * FtpRename - rename a file at remote
 *
 * return 1 if successful, 0 otherwise
 */
int FtpRename(const char* src, const char* dst, NetBuf_t* nControl)
{
	char cmd[FTPLIB_TEMP_BUFFER_SIZE];
	if (((strlen(src) + 7) > sizeof(cmd)) ||
		((strlen(dst) + 7) > sizeof(cmd)))
		return 0;
	sprintf(cmd, "RNFR %s", src);
	if (!sendCommand(cmd, '3', nControl))
		return 0;
	sprintf(cmd,"RNTO %s",dst);
	if (!sendCommand(cmd, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * FtpAccess - return a handle for a data stream
 *
 * return 1 if successful, 0 otherwise
 */
int FtpAccess(const char* path, int typ, int mode, NetBuf_t* nControl,
	NetBuf_t** nData)
{
	if ((path == NULL) &&
		((typ == FTPLIB_FILE_WRITE) || (typ == FTPLIB_FILE_READ))) {
		sprintf(nControl->response,
					"Missing path argument for file transfer\n");
		return 0;
	}
	char buf[FTPLIB_TEMP_BUFFER_SIZE];
	sprintf(buf, "TYPE %c", mode);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	int dir;
	switch (typ) {
		case FTPLIB_DIR:
		{
			strcpy(buf, "NLST");
			dir = FTPLIB_READ;
		}
		break;

		case FTPLIB_DIR_VERBOSE:
		{
			strcpy(buf, "LIST");
			dir = FTPLIB_READ;
		}
		break;

		case FTPLIB_FILE_READ:
		{
			strcpy(buf, "RETR");
			dir = FTPLIB_READ;
		}
		break;

		case FTPLIB_FILE_WRITE:
		{
			strcpy(buf, "STOR");
			dir = FTPLIB_WRITE;
		}
		break;

		case FTPLIB_MLSD:
		{
			strcpy(buf, "MLSD");
			dir = FTPLIB_READ;
		}
		break;

		default:
		{
			sprintf(nControl->response, "Invalid open type %d\n", typ);
			return 0;
		}
	}

	if (path != NULL) {
		int i = strlen(buf);
		buf[i++] = ' ';
		if ((strlen(path) + i + 1) >= sizeof(buf))
			return 0;
		strcpy(&buf[i], path);
	}

	if (openPort(nControl, nData, mode, dir) == -1)
		return 0;
	if (!sendCommand(buf, '1', nControl)) {
		FtpClose(*nData);
		*nData = NULL;
		return 0;
	}
	if (nControl->cmode == FTPLIB_ACTIVE) {
		if (!acceptConnection(*nData,nControl)) {
			FtpClose(*nData);
			*nData = NULL;
			nControl->data = NULL;
			return 0;
		}
	}
	return 1;
}



/*
 * FtpRead - read from a data connection
 */
int FtpRead(void* buf, int max, NetBuf_t* nData)
{
	if (nData->dir != FTPLIB_READ)
		return 0;
	int i = 0;
	if (nData->buf){
		i = readLine(buf, max, nData);
	}
	else {
		i = socketWait(nData);
		if (i != 1)
			return 0;
		i = recv(nData->handle, buf, max, 0);
	}
	if (i == -1)
		return 0;
	nData->xfered += i;
	if (nData->idlecb && nData->cbbytes) {
		nData->xfered1 += i;
		if (nData->xfered1 > nData->cbbytes) {
			if (nData->idlecb(nData, nData->xfered, nData->idlearg) == 0)
				return 0;
			nData->xfered1 = 0;
		}
	}
	return i;
}



/*
 * FtpWrite - write to a data connection
 */
int FtpWrite(const void* buf, int len, NetBuf_t* nData)
{
	int i = 0;
	if (nData->dir != FTPLIB_WRITE)
		return 0;
	if (nData->buf)
		i = writeLine(buf, len, nData);
	else {
		socketWait(nData);
		i = send(nData->handle, buf, len, 0);
	}
	if (i == -1)
		return 0;
	nData->xfered += i;
	if (nData->idlecb && nData->cbbytes) {
		nData->xfered1 += i;
		if (nData->xfered1 > nData->cbbytes) {
			nData->idlecb(nData, nData->xfered, nData->idlearg);
			nData->xfered1 = 0;
		}
	}
	return i;
}



/*
 * FtpClose - close a data connection
 */
int FtpClose(NetBuf_t* nData)
{
	switch (nData->dir)
	{
		case FTPLIB_WRITE:
		case FTPLIB_READ:
			if (nData->buf)
				free(nData->buf);
			shutdown(nData->handle, 2);
			closesocket(nData->handle);
			NetBuf_t* ctrl = nData->ctrl;
			free(nData);
			ctrl->data = NULL;
			if (ctrl && ctrl->response[0] != '4' && ctrl->response[0] != '5')
				return(readResponse('2', ctrl));
			return 1;

		case FTPLIB_CONTROL:
			if (nData->data) {
				nData->ctrl = NULL;
				FtpClose(nData->data);
			}
			closesocket(nData->handle);
			free(nData);
			return 0;
	}
	return 1;
}
