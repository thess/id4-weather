#ifndef ID4_PI_H_INCLUDED
#define ID4_PI_H_INCLUDED

// id4_pi.h
//
// Project include

#define VERSION_MAJOR   0
#define VERSION_MINOR   8

// boolean constants
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

extern int ReSyncID4(void);

extern struct tm    tmLocalTime;
extern time_t       ttLocalTime;
extern int          iTZOffset;
short               sMinutesPastMidnite;

extern const char * const sDayOfWeek[];
extern const char * const sMonTab[];
extern const char * const sAmPmBuff[];
extern const char * const sWinDir[];

extern struct tm tmLocalTime;

extern pthread_mutex_t  id4_mutex;

extern int fPort;
extern int bNonBlock;
extern char sPortName[];

#endif // ID4_PI_H_INCLUDED
