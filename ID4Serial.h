//
// ID4Serial.h
//
// Serial port definitions for Heath ID-4001 Weather Station
//

#ifndef __ID4SERIAL_H
#define __ID4SERIAL_H

extern int QueryIdent(int bShow);
extern int ReadDateTime(unsigned char *sTimeBuf);
extern int SetDateTime(unsigned char sClkMode);
extern int ReadWeather(unsigned char *sWeatherBuf);
extern int ReadMinMaxData(unsigned char *sBuf1, unsigned char *sBuf2);
extern int SendSingleCmd(unsigned char sCmd);
extern int ReadVersion(unsigned char *sVersion);
extern void ShowDateTime(char *sPrefix, unsigned char *sTimeBuf);
extern void ShowWeather(char *sPrefix, unsigned char *sWeatherBuf);
extern void ShowMinMax(void);
extern void ShowHistory(void);
extern void ShowVersion(void);

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

typedef enum
{
    ID4_LOG_WEATHER = 1,
    ID4_LOG_MIDNITE,
    ID4_TIME_SYNC,
    ID4_TIME_SET
} ID4_CMDFUNC;

#define ID4_LOCK()      ID4_Reserve()
#define ID4_UNLOCK()    ID4_Release()

extern void ID4_Reserve(void);
extern void ID4_Release(void);

#endif	// __ID4SERIAL_H
