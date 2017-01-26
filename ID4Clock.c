// ID4Clock.c - Timer processing and clock management

/*
 * Copyright (c) 2014 by Ted Hess
 * Kitschensync - RPi daemon for Heathkit ID4001
 *
 * Portions used from the Webio Open Source lightweight web server.
 * Copyright (c) 2007 by John Bartas
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <mqueue.h>

#include "id4-pi.h"
#include "ID4Serial.h"

extern int ftpUpload(char *srcFile, char *dstFile);

int NewLog(int xTime);
int OpenLog(void);
void CloseLog(void);
void LogMessage(int xTime, char *sMsg);
void LogWeatherData(int xTime);
void LogMinMaxData(int xTime);
void LogMinMax(unsigned char *sBuf1, unsigned char *sBuf2);
void LogCurrentReadings(int xTime, unsigned char *sWeatherBuf);

static FILE *hLog;
static char *sFileBuf;
static char sLogPath[64];
static char sTargetName[16];

#define WEATHER_LOG_HEADER1	"Time,Indoor,Outdoor,Wind,Dir,Pressure\n"
#define WEATHER_LOG_HEADER2	"Midnite,TLow,Time,THigh,Time,Wind,Time,PLow,Time,PHigh,Time\n"
#define NOWEATHER_LOG_HEADER "*** Log start - Weather data disabled\n"

#define LOCAL_LOG_PATH  "/opt/weather"
#define LL_PATHSIZE     (sizeof(LOCAL_LOG_PATH) - 1)

void *xID4Clock(void *args)
{
	mqd_t id4_mq = *(mqd_t *)args;
	int iSaveDST = 0;
	ID4Cmd  xCmd;
	char *xBuf;
	time_t ltime;
	struct tm *timenow;

	// Get current system time
	ltime = time(NULL);
	timenow = localtime(&ltime);

	// Create ID4 weather log
	if (!NewLog((60 * timenow->tm_hour) + timenow->tm_min))
	{
		printf("Log file creation failed: %s\n", strerror(errno));
		return NULL;
	}

	while (TRUE)
	{
		if (mq_receive(id4_mq, (char *)&xCmd, ID4CMD_SIZE, NULL) < 0)
		{
			printf("mq_receive returned %d (%s)\n", errno, strerror(errno));
			break;
		}

		// Service request
		printf("->ID4 command: %d at %d\n", xCmd.cmd, xCmd.time);

		// Process item
		switch(xCmd.cmd)
		{
		case ID4_LOG_WEATHER:
			if (bLogWeather)
				LogWeatherData(xCmd.time);
			break;

		case ID4_LOG_MIDNITE:
			if (bLogWeather)
			{
				// Log min/max from past 24hrs
				LogMinMaxData(xCmd.time);

				// Strip log path prefix
				strcpy(sTargetName, &sLogPath[LL_PATHSIZE]);
				// Flatten name, Ex: /May02/12 -> wMay02-12
				xBuf = &sTargetName[0];
				*xBuf = 'w';
				while ((xBuf = strchr(xBuf, '/')))
					*xBuf = '-';

				// Send current log to FTP server
				printf("MIDNITE: Upload to: %s\n", sTargetName);
				ftpUpload(sLogPath, sTargetName);
			}

			// Start new log w/current weather (midnite implied)
			if (!NewLog(0))
				printf("Log file creation failed: %s\n", strerror(errno));

			if (bLogWeather)
				LogWeatherData(0);

			// Sync up date/time
			printf("MIDNITE: Clock set\n");
			if (SetDateTime('6') < 0)
				LogMessage(xCmd.time, "--Set clock failed--\n");


			// Reset weather data
			printf("MIDNITE: Reset weather min/max data\n");
			ID4_LOCK();
			SendSingleCmd('C');
			ID4_UNLOCK();
			// For time sync check
			iSaveDST = tmLocalTime.tm_isdst;
			printf("MIDNITE: Done\n");

			break;

		case ID4_TIME_SYNC:
#if defined(DEBUG)
			printf("%% Time sync: %s %2d %s %d, ", sDayOfWeek[tmLocalTime.tm_wday],
				   tmLocalTime.tm_mday,
				   sMonName[tmLocalTime.tm_mon],
				   tmLocalTime.tm_year + 1900);
			printf("%02d:%02d:%02d\n", tmLocalTime.tm_hour, tmLocalTime.tm_min, tmLocalTime.tm_sec);
#endif
			// Check for DST change since last time sync
			if (iSaveDST != tmLocalTime.tm_isdst)
			{
				if (SetDateTime('6') < 0)
					LogMessage(xCmd.time, "--Set clock DST failed--\n");
				iSaveDST = tmLocalTime.tm_isdst;
			}
			break;

		case ID4_TIME_SET:
			if (SetDateTime('6') < 0)
				LogMessage(xCmd.time, "--Set clock failed--\n");
			iSaveDST = tmLocalTime.tm_isdst;
			break;

		default:
			printf("?Bogus request: %d\n", xCmd.cmd);
			break;
		}

	}

	return NULL;
}

//
// Output message to log file
//
void LogMessage(int xTime, char *sMsg)
{
	size_t nCnt;

	// Open log file for append
	if (OpenLog())
	{
		nCnt = sprintf(sFileBuf, "%d,%s", xTime, sMsg);
		fwrite(sFileBuf, nCnt, 1, hLog);
		CloseLog();
		printf("LOG: %s", sFileBuf);
	}

	return;
}

//
// Weather log file handling
//

// Open/Append current log file
int OpenLog(void)
{
	hLog = fopen(sLogPath, "a");
	if (hLog == NULL)
	{
		printf("Log open failed: %s\n", strerror(errno));
		return FALSE;
	}

	// Allocate write buffer
	sFileBuf = (char *)malloc(128);
	if (!sFileBuf)
	{
		CloseLog();
		return FALSE;
	}

	return TRUE;
}

// Close current log file
void CloseLog(void)
{
	// Close log file
	if (hLog)
	{
		fclose(hLog);
		hLog = NULL;
	}
	// Free buffer
	if (sFileBuf)
	{
		free(sFileBuf);
		sFileBuf = NULL;
	}

	return;
}

// Create new, empty, daily log
int NewLog(int xTime)
{
	struct stat	xInfo;
	size_t	nOut;
	int		nRes;
	int		nRet = FALSE;

	// Create path name from date (/mmmyy)
	nOut = sprintf(sLogPath, LOCAL_LOG_PATH "/%s%02d", sMonName[tmLocalTime.tm_mon], tmLocalTime.tm_year - 100);
	if (stat(sLogPath, &xInfo))
	{
		if ((errno == ENOTDIR) || (errno == ENOENT))
		{
			// Create path if non-existing
			if (mkdir(sLogPath, 0755) == 0)
				nRet = TRUE;
		}
	}
	else
	{
		// Must be a directory
		nRet = TRUE;
	}

	// nRet := TRUE if path OK
	if (nRet)
	{
		nRet = FALSE;
		// Create file name from date (/mmmyy/dd)
		sprintf(&sLogPath[nOut], "/%02d", tmLocalTime.tm_mday);
		// Create file
		hLog = fopen(sLogPath, "a+");
		if (hLog != NULL)
		{
			// Check if new file
			stat(sLogPath, &xInfo);
			if (xInfo.st_size == 0)
			{
				if (bLogWeather)
				{
					// Write weather header
					fwrite(WEATHER_LOG_HEADER1, sizeof(WEATHER_LOG_HEADER1) - 1, 1, hLog);
				}
				else
				{
					// No weather date logging
					fwrite(NOWEATHER_LOG_HEADER, sizeof(NOWEATHER_LOG_HEADER) -1 , 1, hLog);
				}
			}
			else
			{
				// No message if no time
				if (xTime >= 0)
				{
					// Allocate temp write buffer
					sFileBuf = (char *)malloc(40);
					if (sFileBuf)
					{
						// Append restart data
						nRes = sprintf(sFileBuf, "%d,--System restart--\n", xTime);
						fwrite(sFileBuf, nRes, 1, hLog);
					}
				}
			}
			nRet = TRUE;
		}
	}

	// Close file and release FS (ready for logging)
	CloseLog();

	return nRet;
}
static int x24hr(unsigned char nHour)
{
	unsigned char xPM = nHour & 0x80;
	// Strip AM/PM indicator
	nHour &= 0x7F;
	// 12am
	if ((xPM == 0) && (nHour == 12))
		return 0;
	// 1pm to 11pm
	if ((xPM != 0) && (nHour != 12))
		return nHour + 12;

	// 1am to 12pm
	return nHour;
}

void LogMinMaxData(int xTime)
{
	int rc;
	unsigned char sWBuf1[MMTEMP_BUF_SIZE], sWBuf2[MMPRES_BUF_SIZE];

	do
	{
		rc = ReadMinMaxData(sWBuf1, sWBuf2);
		if (rc)
		{
			if (ReSyncID4())
				break;
			// Resync OK - retry
			continue;
		}
	}
	while (rc != 0);

	if (rc == 0)
	{
		LogMinMax(sWBuf1, sWBuf2);
	}
	else
	{
		LogMessage(xTime, "--No MinMax data--\n");
	}

	return;
}

// Read and log current weather data
void LogWeatherData(int xTime)
{
	int rc = 0;
	unsigned char *sWBuf;

	sWBuf = (unsigned char *)malloc(WEATHER_BUF_SIZE);
	if (sWBuf)
	{
		do
		{
			rc = ReadWeather(sWBuf);
			if (rc)
			{
				if (ReSyncID4())
					break;
				// Resync OK - retry
				continue;
			}
		}
		while (rc != 0);

		// check success
		if (rc == 0)
		{
			LogCurrentReadings(xTime, sWBuf);
		}
		else
		{
			LogMessage(xTime, "--No weather--\n");
		}
		free(sWBuf);
	}

	return;
}

void LogMinMax(unsigned char *sBuf1, unsigned char *sBuf2)
{
	int     nPres1, nPres2;
	size_t  nCnt;

	// Open log file for append
	if (OpenLog())
	{
		// Write min/max header
		fwrite(WEATHER_LOG_HEADER2, sizeof(WEATHER_LOG_HEADER2) - 1, 1, hLog);

		// sbuf1 contains temp high/low & wind speed
		// Log Tlow/Thigh (1440 := midnite)
		nCnt = sprintf(sFileBuf, "1440,%d,%02d:%02d,%d,%02d:%02d", sBuf1[1] - 40, x24hr(sBuf1[3]), sBuf1[2],
					   sBuf1[6] - 40, x24hr(sBuf1[8]), sBuf1[7]);

		// Log Wind
		nCnt += sprintf(&sFileBuf[nCnt], ",%d,%02d:%02d", (sBuf1[11] * 99) / 256, x24hr(sBuf1[13]), sBuf1[12]);

		// sBuf2 contains barometric pressure high/low data
		nPres1 = (sBuf2[1] + 2900) / 100;
		nPres2 = (sBuf2[1] + 2900) - (nPres1 * 100);

		// Log Plow
		nCnt += sprintf(&sFileBuf[nCnt], ",%d.%02d,%02d:%02d", nPres1, nPres2, x24hr(sBuf2[3]), sBuf2[2]);

		nPres1 = (sBuf2[6] + 2900) / 100;
		nPres2 = (sBuf2[6] + 2900) - (nPres1 * 100);

		// Log Phigh
		nCnt += sprintf(&sFileBuf[nCnt], ",%d.%02d,%02d:%02d\n", nPres1, nPres2, x24hr(sBuf2[8]), sBuf2[7]);

		fwrite(sFileBuf, nCnt, 1, hLog);
		CloseLog();
	}

	return;
}

void LogCurrentReadings(int xTime, unsigned char *sWeatherBuf)
{
	int nPres1, nPres2, nWinDir;
	size_t nCnt;

	// Open log file for append
	if (OpenLog())
	{
		// Convert presure
		nPres1 = (sWeatherBuf[11] + 2900) / 100;
		nPres2 = (sWeatherBuf[11] + 2900) - (nPres1 * 100);

		// Fold input 4-bit gray code to table index (wind direction)
		nWinDir = (sWeatherBuf[1] & 0x1C) >> 2;
		nWinDir |= (sWeatherBuf[1] & 0x80) >> 4;
		// time, indoor, outdoor, Wind, Direction, Pressure
		nCnt = sprintf(sFileBuf, "%d,%d,%d,%d,%s,%d.%02d\n", xTime, sWeatherBuf[9] - 40, sWeatherBuf[10] - 40,
					   (sWeatherBuf[8] * 99) / 256, sWinDir[nWinDir], nPres1, nPres2);
		fwrite(sFileBuf, nCnt, 1, hLog);
		CloseLog();
	}

	return;
}

