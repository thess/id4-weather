// ID4Serial.c - Communicate with ID4001-5 serial port

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "id4-pi.h"
#include "ID4Serial.h"
#include "serport.h"

//
// A few notes about input values
//
// Temp - Offset from -40F
// Pres - Offset from 2900 (.01 in)
// Wind - (v * 99) / 256
//

//
// AM/PM string function
//
const char *sAmPm(unsigned char nHour)
{
    return sAmPmBuff[((nHour & 0x80) >> 7) & 1];
}
//
// Month name function (bounds check)
//
const char *sMonTab(unsigned char nMon)
{
    if (nMon > 11)
        return "Ovfl";

    return sMonName[nMon];
}
#if defined(RECORD_MODE)
static void DumpResponseToFile(FILE *fLog, char cmd, unsigned char *sBuf, int nSize)
{
    int j;

    fprintf(fLog, RESP_HDR, cmd, nSize);

    for (j= 0; j < (nSize - 1); j++)
    {
        fprintf(fLog, "0x%02X, ", sBuf[j]);
    }

    fprintf(fLog, "0x%02X\n};\n\n", sBuf[nSize -1]);

    return;
}

#endif

//
// Send and verify single letter command
//
int SendSingleCmd(unsigned char sCmd)
{
    int nErr;
    unsigned char sCmdBuf;

    nErr = WriteSerPort(fPort, &sCmd, 1);
    if (nErr < 0)
        return nErr;

    // Verify command
    nErr = ReadSerPort(fPort, &sCmdBuf, 1, sCmd);
    if (nErr < 0)
        return nErr;

    return 0;
}

//
// Set ID4001 to 60hz clock mode,
// Set current date-time from PC
//
int SetDateTime(unsigned char sClkMode)
{
    int rc = 0;
    int nRet;
    unsigned char nHour;
    unsigned char bAmPm;
    unsigned char sCmdBuf[4];
    time_t ltime;
    struct tm *timenow;

    ID4_LOCK();

    do
    {
        // -------- Set Clock ----------------

        // Get current system date/time
        ltime = time(NULL);
        timenow = localtime(&ltime);

        // Use current time
        sCmdBuf[0] = 't';
        sCmdBuf[1] = timenow->tm_sec;
        sCmdBuf[2] = timenow->tm_min;
        nHour = timenow->tm_hour;
        bAmPm = 0;
        // Adjust to 12-hr time w/AM-PM
        if (nHour > 11)
        {
            nHour -= 12;
            bAmPm = 0x80;
        }

        if (nHour == 0)
            nHour = 12;

        // Save result
        sCmdBuf[3] = nHour | bAmPm;

        // Set time
        nRet = WriteSerPort(fPort, sCmdBuf, 4);
        if (nRet < 0)
            break;

        // Verify command
        nRet = ReadSerPort(fPort, sCmdBuf, 1, 0);
        if (nRet < 0)
            break;

        if (sCmdBuf[0] != 't')
        {
            printf("*** Set time failed\n");
            rc = -1;
            break;
        }

        // Wait 100ms between commands
        usleep(100 * 1000);

        sCmdBuf[0] = 'd';
        sCmdBuf[1] = timenow->tm_mday;
        sCmdBuf[2] = timenow->tm_mon + 1;
        sCmdBuf[3] = timenow->tm_year - 100;

        // Set Date
        nRet = WriteSerPort(fPort, sCmdBuf, 4);
        if (nRet < 0)
            break;

        // Verify command
        nRet = ReadSerPort(fPort, sCmdBuf, 1, 0);
        if (nRet < 0)
            break;

        if (sCmdBuf[0] != 'd')
        {
            printf("*** Set date failed\n");
            rc = -1;
            break;
        }

    }
    while(FALSE);

    ID4_UNLOCK();

    return (nRet < 0) ? nRet : rc;
}

//
// Dump date-time info into single buffer
//
int ReadDateTime(unsigned char *sTimeBuf)
{
    int nRet;
    int rc = 0;
    unsigned char sCmd;

    ID4_LOCK();

    do
    {
        sCmd = 'T';
        nRet = WriteSerPort(fPort, &sCmd, 1);
        if (nRet < 0)
            break;

        nRet = ReadSerPort(fPort, sTimeBuf, 4, sCmd);
        if (nRet < 0)
            break;

#if defined(RECORD_MODE)
        if (bRecording)
            DumpResponseToFile(fLog, 'T', sTimeBuf, 4);
#endif

        sCmd = 'D';
        nRet = WriteSerPort(fPort, &sCmd, 1);
        if (nRet < 0)
            return -1;

        nRet = ReadSerPort(fPort, &sTimeBuf[4], 4, sCmd);
        if (nRet < 0)
            break;

#if defined(RECORD_MODE)
        if (bRecording)
            DumpResponseToFile(fLog, 'D', &sTimeBuf[4], 4);
#endif
    }
    while(FALSE);

    ID4_UNLOCK();

    return (nRet < 0) ? nRet : rc;
}

//
// Fetch current weather data
//
int ReadWeather(unsigned char *sWeatherBuf)
{
    int nRet;
    unsigned char sCmd;

    ID4_LOCK();

    do
    {
        sCmd = 'W';
        nRet = WriteSerPort(fPort, &sCmd, 1);
        if (nRet < 0)
            break;

        nRet = ReadSerPort(fPort, sWeatherBuf, 17, sCmd);
        if (nRet < 0)
            break;
#if defined(RECORD_MODE)
        if (bRecording)
            DumpResponseToFile(fLog, 'W', sWeatherBuf, 17);
#endif
    }
    while(FALSE);

    ID4_UNLOCK();

    return (nRet < 0) ? nRet : 0;
}

//
// Fetch current min/max data
//
int ReadMinMaxData(unsigned char *sBuf1, unsigned char *sBuf2)
{
    int nRet;
    unsigned char sCmd;

    ID4_LOCK();

    do
    {
        sCmd = 'e';
        nRet = WriteSerPort(fPort, &sCmd, 1);
        if (nRet < 0)
            break;

        nRet = ReadSerPort(fPort, sBuf1, MMTEMP_BUF_SIZE, sCmd);
        if (nRet < 0)
            break;

        usleep(100 * 1000);

        sCmd = 'b';
        nRet = WriteSerPort(fPort, &sCmd, 1);
        if (nRet < 0)
            break;

        nRet = ReadSerPort(fPort, sBuf2, MMPRES_BUF_SIZE, sCmd);
        if (nRet < 0)
            break;

    }
    while(FALSE);

    ID4_UNLOCK();

    return (nRet < 0) ? nRet : 0;
}

//
// Display date-time in US. 12hr format
//
void ShowDateTime(char *sPrefix, unsigned char *sTimeBuf)
{
    printf("%s%2d:%02d:%02d %s", sPrefix, sTimeBuf[3] & 0x7F, sTimeBuf[2], sTimeBuf[1], sAmPm(sTimeBuf[3]));

    // Have date in form <dd><mm><yy - 2000>
    printf("  %d-%s-%d\n", sTimeBuf[5], sMonTab(sTimeBuf[6] - 1), sTimeBuf[7] + 2000);

    return;
}

//
// Display current weather summary
//
void ShowWeather(char *sPrefix, unsigned char *sWeatherBuf)
{
    int nPres1, nPres2, nWinDir;

    // Convert presure
    nPres1 = (sWeatherBuf[11] + 2900) / 100;
    nPres2 = (sWeatherBuf[11] + 2900) - (nPres1 * 100);

    // Fold input 4-bit gray code to table index (wind direction)
    nWinDir = (sWeatherBuf[1] & 0x1C) >> 2;
    nWinDir |= (sWeatherBuf[1] & 0x80) >> 4;

    printf("%s%2d:%02d:%02d %s", sPrefix,
           sWeatherBuf[4] & 0x7F, sWeatherBuf[3], sWeatherBuf[2], sAmPm(sWeatherBuf[4]));
    printf("  %d-%s-%d\n", sWeatherBuf[5], sMonTab(sWeatherBuf[6] - 1), sWeatherBuf[7] + 2000);
    printf("Wind direction: %s, Speed: %d, Indoor: %d, Outdoor: %d, Pressure: %d.%02d\n",
           sWinDir[nWinDir], (sWeatherBuf[8] * 99) / 256, sWeatherBuf[9] - 40, sWeatherBuf[10] - 40, nPres1, nPres2);

    return;
}

//
// Fetch and display current min-max data w/ date-time info
//
void ShowMinMax(void)
{
    int rc;
    int nPres1, nPres2;
    unsigned char sMinMax1[MMTEMP_BUF_SIZE], sMinMax2[MMPRES_BUF_SIZE];

    do
    {
        rc = ReadMinMaxData(sMinMax1, sMinMax2);
        if (rc)
        {
            if (ReSyncID4())
                break;
            // Resync OK - retry
            continue;
        }
    }
    while (rc != 0);

    if (rc != 0)
    {
        printf("No data available\n");
        return;
    }

    if ((sMinMax1[5] > 11) || (sMinMax1[10] > 11) || (sMinMax1[15] > 12))
    {
        printf("Bad temp/wind data: %d, %d, %d\n", sMinMax1[5], sMinMax1[10], sMinMax1[15]);
        return;
    }

    printf("Tlow = %d on %d-%s %d:%02d %s,", sMinMax1[1] - 40, sMinMax1[4], sMonTab(sMinMax1[5] - 1),
           sMinMax1[3] & 0x7F, sMinMax1[2], sAmPm(sMinMax1[3]));

    printf("  Thigh = %d on %d-%s %d:%02d %s\n", sMinMax1[6] - 40, sMinMax1[9], sMonTab(sMinMax1[10] - 1),
           sMinMax1[8] & 0x7F, sMinMax1[7], sAmPm(sMinMax1[8]));
    ;
    printf("Wspeed = %d on %d-%s %d:%02d %s\n", (sMinMax1[11] * 99) / 256, sMinMax1[14], sMonTab(sMinMax1[15] - 1),
           sMinMax1[13] & 0x7F, sMinMax1[12], sAmPm(sMinMax1[13]));


    nPres1 = (sMinMax2[1] + 2900) / 100;
    nPres2 = (sMinMax2[1] + 2900) - (nPres1 * 100);

    if ((sMinMax2[5] > 12) || (sMinMax2[10] > 12))
    {
        printf("Bad pressure data: %d, %d\n", sMinMax2[5], sMinMax2[10]);
        return;
    }

    printf("Plow = %d.%02d on %d-%s %d:%02d %s,  ", nPres1, nPres2, sMinMax2[4], sMonTab(sMinMax2[5] - 1),
           sMinMax2[3] & 0x7F, sMinMax2[2], sAmPm(sMinMax2[3]));

    nPres1 = (sMinMax2[6] + 2900) / 100;
    nPres2 = (sMinMax2[6] + 2900) - (nPres1 * 100);

    printf("Phigh = %d.%02d on %d-%s %d:%02d %s\n", nPres1, nPres2, sMinMax2[9], sMonTab(sMinMax2[10] - 1),
           sMinMax2[8] & 0x7F, sMinMax2[7], sAmPm(sMinMax2[8]));

    return;
}

//
// Display last 31 days weather high/lows
//
void ShowHistory(void)
{
    int nErr;
    int nPres1, nPres2;
    int n;

    unsigned char sCmd;
    unsigned char sHistory[466];
    unsigned char *sRecord;

    ID4_LOCK();

    sCmd = 'i';
    nErr = WriteSerPort(fPort, &sCmd, 1);
    if (nErr < 0)
    {
        ID4_UNLOCK();
        return;
    }

    nErr = ReadSerPort(fPort, sHistory, 466, sCmd);
    if (nErr < 0)
    {
        ID4_UNLOCK();
        return;
    }

    ID4_UNLOCK();

    for (n = 0; n < 31; n++)
    {
        sRecord = &sHistory[15 * n];
        printf("Day %d:\tTlow = %d at %d:%02d %s, Thigh = %d at %d:%02d %s\n", n + 1,
               sRecord[1] - 40, sRecord[3] & 0x7F, sRecord[2], sAmPm(sRecord[3]),
               sRecord[4] - 40, sRecord[6] & 0x7F, sRecord[5], sAmPm(sRecord[6]));
        printf("\tWspeed = %d at %d:%02d %s\n",
               (sRecord[13] * 99) / 256, sRecord[15] & 0x7F, sRecord[14], sAmPm(sRecord[15]));

        nPres1 = (sRecord[7] + 2900) / 100;
        nPres2 = (sRecord[7] + 2900) - (nPres1 * 100);

        printf("\tPlow = %d.%02d at %d:%02d %s, ", nPres1, nPres2,
               sRecord[9] & 0x7F, sRecord[8], sAmPm(sRecord[9]));

        nPres1 = (sRecord[10] + 2900) / 100;
        nPres2 = (sRecord[10] + 2900) - (nPres1 * 100);

        printf("Phigh = %d.%02d at %d:%02d %s\n", nPres1, nPres2,
               sRecord[12] & 0x7F, sRecord[11], sAmPm(sRecord[12]));
    }

    return;
}

int ReadVersion(unsigned char *sVersion)
{
    int nRet;
    unsigned char sCmd;

    ID4_LOCK();

    sCmd = 'v';
    nRet = WriteSerPort(fPort, &sCmd, 1);
    if (nRet < 0)
    {
        ID4_UNLOCK();
        return -1;
    }

    nRet = ReadSerPort(fPort, sVersion, 4, sCmd);
    if (nRet < 0)
    {
        ID4_UNLOCK();
        return -1;
    }

    ID4_UNLOCK();

    return 0;
}

void ShowVersion(void)
{
    unsigned char sVersion[4];

    if (ReadVersion(sVersion))
        return;

    printf("Version = %c%d.%d-%d\n", sVersion[0], sVersion[1], sVersion[2], sVersion[3]);

#if defined(RECORD_MODE)
    if (bRecording)
        DumpResponseToFile(fLog, 'v', sVersion, sizeof(sVersion));
#endif

    return;
}

// Serialize access to port
void ID4_Reserve(void)
{
    pthread_mutex_lock(&id4_mutex);

    fPort = OpenSerPort(sPortName);
    if (fPort < 0)
    {
        printf("Serial port open error: %d, %s\n", errno, strerror(errno));
        pthread_mutex_unlock(&id4_mutex);
    }

    return;
}

void ID4_Release(void)
{
    CloseSerPort(fPort);
    fPort = -1;

    pthread_mutex_unlock(&id4_mutex);
    return;
}
