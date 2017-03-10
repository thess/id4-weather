#ifndef ID4_PI_H_INCLUDED
#define ID4_PI_H_INCLUDED

// id4_pi.h
//
// Project include

#include "config.h"

// boolean constants
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// Remove comment below to force ONION build
// #define ONION

#if defined(ONION)
#include <oled-exp.h>
#define OBUFSIZE	128
#endif

extern int ReSyncID4(void);

extern struct tm    tmLocalTime;
extern time_t       ttLocalTime;
extern int          iTZOffset;
short               sMinutesPastMidnite;

extern const char * const sDayOfWeek[];
extern const char * const sMonName[];
extern const char * const sAmPmBuff[];
extern const char * const sWinDir[];

extern struct tm tmLocalTime;

extern pthread_mutex_t  id4_mutex;

extern int fPort;
extern char sPortName[];
extern char *sWLogPath;

extern int bLogWeather;
extern int bWebEnable;

#if defined(ONION)
extern int bOnionDpy;
extern char oledBuf[];
#endif

#endif // ID4_PI_H_INCLUDED
