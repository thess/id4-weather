//
// ID4Serial.h
//
// Serial port definitions for Heath ID-4001 Weather Station
//

#ifndef __ID4SERIAL_H
#define __ID4SERIAL_H

extern int QueryIdent(int fPort, int bShow);
extern int ReadDateTime(int fPort, unsigned char *sTimeBuf);
extern int SetDateTime(int fPort, unsigned char sClkMode);
extern int ReadWeather(int fPort, unsigned char *sWeatherBuf);
extern int ReadMinMaxData(int fPort, unsigned char *sBuf1, unsigned char *sBuf2);
extern int SendSingleCmd(int fPort, unsigned char sCmd);
extern int ReadVersion(int fPort, unsigned char *sVersion);
extern void ShowDateTime(char *sPrefix, unsigned char *sTimeBuf);
extern void ShowWeather(char *sPrefix, unsigned char *sWeatherBuf);
extern void ShowMinMax(int fPort);
extern void ShowHistory(int fPort);
extern void ShowVersion(int fPort);

// Define various weather data buffer sizes
#define WEATHER_BUF_SIZE	17
#define MMTEMP_BUF_SIZE		16
#define MMPRES_BUF_SIZE		11

//
// ID4 Command item
//
typedef struct _ID4CmdItem
{
        short           time;       // Trigger time in minutes past midnite
        unsigned char   cmd;        // ID4 command request
} ID4Cmd;

#define ID4CMD_SIZE     sizeof(struct _ID4CmdItem)
#define ID4_QUEUE_SIZE  10

typedef enum {
        ID4_LOG_WEATHER = 1,
        ID4_LOG_MIDNITE,
        ID4_TIME_SYNC,
        ID4_TIME_SET
} ID4_CMDFUNC;

#define ID4_LOCK    pthread_mutex_lock(&id4_mutex)
#define ID4_UNLOCK    pthread_mutex_unlock(&id4_mutex)

#endif	// __ID4SERIAL_H
