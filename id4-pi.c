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
#include <unistd.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <mqueue.h>

#include "id4-pi.h"
#include "serport.h"
#include "ID4Serial.h"

const char * const sMonTab[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
								   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
								 };

// Wind direction - gray coded
const char * const sWinDir[16] = { "N", "NNW", "WNW", "NW", "SSW", "SW", "W", "WSW",
								   "NNE", "NE", "E", "ENE", "S", "SSE", "ESE", "SE"
								 };

// AM / PM strings
const char * const sAmPmBuff[2] = { "AM", "PM" };

// Day of week map
const char * const sDayOfWeek[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

const unsigned char sFirmware[4] = {'v', 5, 6, 28};

// TOD clock
struct tm       tmLocalTime;
time_t          ttLocalTime;
int             iTZOffset;
short           sMinutesPastMidnite;

// Timer definitions
#define ID4SIG         SIGRTMIN

timer_t     id4timerid;

// Serial port for ID4001
int fPort;
int bNonBlock;

// Local vars (command options)
char sPortName[16];
int bDaemonize;
int bWebEnable;
int bLogWeather;
static int cImmediate;

// Threads, mutexes and queues
pthread_t   tWebIO;
extern void *xWebStart(void *);

pthread_t   tID4Clock;
extern void *xID4Clock(void *);

// Serialze access to serial device
pthread_mutex_t id4_mutex;

// Timer message queue
mqd_t id4_mq;

#define MQ_NAME	"/ID4_MQ"

#if defined(RECORD_MODE)
// File handle for logging
FILE *fLog;
int bRecording;

#endif // defined
//-------------------------------------------------------------------------------

// Timer proc -- called from 1S recurring timer
static void do_timer_proc(int sig, siginfo_t *si, void *uc)
{
	int rc = 0;
	ID4Cmd xCmd;

	// make sure this is for us
	if (si->si_value.sival_ptr != &id4timerid)
	{
		printf("!Spurious signal\n");
		return;
	}

	ttLocalTime = time(NULL);
	localtime_r(&ttLocalTime, &tmLocalTime);
	sMinutesPastMidnite = (60 * tmLocalTime.tm_hour) + tmLocalTime.tm_min;

	xCmd.time = sMinutesPastMidnite;

	// Once per hour processing
	if (tmLocalTime.tm_min == 0)
	{
		// Handle midnite special
		if (tmLocalTime.tm_hour == 0)
		{
			// Log and clear min-max, sync time
			xCmd.cmd = ID4_LOG_MIDNITE;
			rc = mq_send(id4_mq, (char *)&xCmd, ID4CMD_SIZE, 0);
		}
		else
		{
			// Send log current weather
			xCmd.cmd = ID4_LOG_WEATHER;
			rc = mq_send(id4_mq, (char *)&xCmd, ID4CMD_SIZE, 0);
		}
	}
	else
	{
		// Log weather every 20 min
		if ((tmLocalTime.tm_min == 20) ||
				(tmLocalTime.tm_min == 40))
		{
			// Send log current weather
			xCmd.cmd = ID4_LOG_WEATHER;
			rc = mq_send(id4_mq, (char *)&xCmd, ID4CMD_SIZE, 0);
		}

	}

	// Read RTC and re-sync time (1 min past hour)
	if (tmLocalTime.tm_min == 1)
	{
		// Read and set system time
		xCmd.cmd = ID4_TIME_SYNC;
		rc = mq_send(id4_mq, (char *)&xCmd, ID4CMD_SIZE, 0);
	}

	if (rc)
	{
		printf("Fatal: mq_send failed: %s\n", strerror(errno));
		if (bWebEnable)
			pthread_cancel(tWebIO);
	}

	return;
}

//-------------------------------------------------------------------------------

int ReSyncID4(void)
{
	int k;
	unsigned char sVers[4];

	printf("Resyncing...");

	for (k = 1; k < 3; k++)
	{
		// Close and reopen serial port
		CloseSerPort(fPort);
		sleep(1);
		fPort = OpenSerPort(sPortName, bNonBlock);
		if (fPort < 0)
			break;
		// Read and verify version
		if (ReadVersion(sVers) == 0)
		{
			if (memcmp(sVers, sFirmware, 4) != 0)
			{
				printf ("again...");
				continue;
			}
			// OK response
			printf("OK\n");
			return 0;
		}
	}

	printf("Failed\n");

	return -1;
}
//-------------------------------------------------------------------------------

void ShowHelp(void)
{
	printf("id4-pi Control and reporting for Heath ID4001 V%d.%d\n\n", VERSION_MAJOR, VERSION_MINOR);
	printf("id4-pi [options]\n\n");
	printf(" options:\n");
	printf("   -s name     Serial device suffix (default: USB0)\n");
	printf("   -W          Show current time/weather data and exit\n");
	printf("   -M          Show lastest min/max data and exit\n");
	printf("   -H          Show weather history (31 days) and exit\n");
	printf("   -V          Show ID4001-5 firmware version and exit\n");
	printf("   -T          Set ID4001 time from system\n");
	printf("   -C          Clear current weather data memory\n");
	printf("   -B          Run in background (daemonize)\n");
	printf("   -Z          Turn off weather logging\n");
	printf("   -n          Open serial port in non-block mode\n");
	printf("   -r          Record serial comms to file: weather.log\n");

	return;
}

void parse_options(int argc, char **argv)
{
	int opt;

	optind = 0;
	while ((opt = getopt(argc, argv, "?Bhs:CTWVMHnrZ")) != -1)
	{
		switch (opt)
		{
		case 'B':
			bDaemonize = TRUE;
			break;

		case 's':
			if (strlen(optarg) > 15)
			{
				printf("Port name too long: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			// Save port name
			strcpy(sPortName, optarg);
			break;

		// Immediate commands
		case 'C':
		case 'T':
		case 'W':
		case 'H':
		case 'V':
		case 'M':
			cImmediate = opt;
			break;

#if defined(RECORD_MODE)
		case 'r':
			bRecording = TRUE;
			break;
#endif // defined

		case 'Z':
			bLogWeather = FALSE;
			break;

		case 'n':
			bNonBlock = TRUE;
			break;

		// Give help and quit
		case 'h':
		case '?':
			ShowHelp();
			exit(EXIT_SUCCESS);

		// Unrecognized option - give help and fail
		default:
			ShowHelp();
			exit(EXIT_FAILURE);
		}
	}

	return;
}

int main(int argc, char **argv)
{
	int rc;
	struct mq_attr mqAttr;
	unsigned char sTimeBuf[8];
	unsigned char sWeatherBuf[17];
	ID4Cmd xCmd;

	struct sigaction sa;
	struct sigevent sev;
	struct itimerspec its;

	bDaemonize = FALSE;
	bWebEnable = TRUE;
	bLogWeather = TRUE;

#if defined(RECORD_MODE)
	bRecording = FALSE;
#endif
	strcpy(sPortName, "USB0");
	cImmediate = 0;
	fPort = -1;
	bNonBlock = FALSE;

	parse_options(argc, argv);

	// Check for too many args
	if (argc > optind)
	{
		printf("Too many args supplied\n");
		ShowHelp();
		exit(EXIT_FAILURE);
	}

	// Blocking open
	fPort = OpenSerPort(sPortName, bNonBlock);
	if (fPort < 0)
		return 1;

	// Init serial ID4 interface mutex
	pthread_mutex_init(&id4_mutex, NULL);

	// Close (for now)

	CloseSerPort(fPort);
	fPort = -1;
#if defined(RECORD_MODE)
	if (bRecording)
	{
		fLog = fopen(WLOG_PATH "/weather.log", "a");
		if (fLog < 0)
		{
			printf("Cannot open/append log file\n");
			return 1;
		}
	}
#endif

	// Check for immediate command options
	switch (cImmediate)
	{
	case'T':
		// Set ID4001 time to system time
		rc = ReadDateTime(sTimeBuf);
		if (rc != 0)
			exit(EXIT_FAILURE);

		ShowDateTime("Time before: ", sTimeBuf);

		printf("Setting date-time to system time...\n");
		SetDateTime('6');
		rc = ReadDateTime(sTimeBuf);
		if (rc != 0)
			exit(EXIT_FAILURE);

		ShowDateTime("Time now: ", sTimeBuf);
		exit(EXIT_SUCCESS);

	case 'W':
		// Display weather info and exit
		do
		{
			rc = ReadWeather(sWeatherBuf);
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
			exit(EXIT_FAILURE);

		ShowWeather("Current readings: ", sWeatherBuf);
		exit(EXIT_SUCCESS);

	case 'H':
		ShowHistory();
		exit(EXIT_SUCCESS);

	case 'M':
		ShowMinMax();
		exit(EXIT_SUCCESS);

	case 'V':
		ShowVersion();
		exit(EXIT_SUCCESS);

	case 'C':
		// Reset weather data
		ID4_LOCK();
		SendSingleCmd('C');
		ID4_UNLOCK();
		exit(EXIT_SUCCESS);

	// Ignore all others
	default:
		break;
	}

	// Run in background?
	if (bDaemonize)
	{
		printf("Detaching...\n");
		rc = daemon(0, TRUE);
		// Continue even if detach error
		if (rc < 0)
			printf("Unable to run in background - %s\n", strerror(errno));
	}

	// Force line flush on stdout (redirection)
	setvbuf(stdout, NULL, _IONBF, 0);
	setlinebuf(stdout);

	// Remove (left-over) queue
	mq_unlink(MQ_NAME);

	// initialize the queue attributes
	mqAttr.mq_flags = 0;
	mqAttr.mq_maxmsg = ID4_QUEUE_SIZE;
	mqAttr.mq_msgsize = sizeof(ID4CMD_SIZE);
	mqAttr.mq_curmsgs = 0;

	// create the message queue
	id4_mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &mqAttr);
	if (id4_mq < 0)
	{
		printf("mq_open error %d %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (bWebEnable)
	{
		// Invoke web server
		if (pthread_create(&tWebIO, NULL, &xWebStart, NULL))
		{
			printf("WebIO thread create failure: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	// Thread to process timer messages
	if (pthread_create(&tID4Clock, NULL, &xID4Clock, (void *)&id4_mq))
	{
		printf("ID4Func thread create failure: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Startup -- sync clock to system time
	xCmd.cmd = ID4_TIME_SET;
	xCmd.time = 0;
	if (mq_send(id4_mq, (char *)&xCmd, ID4CMD_SIZE, 0))
	{
		printf("mq_send failure: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}


	// Setup timer signal hander
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = do_timer_proc;
	sigemptyset(&sa.sa_mask);

	if (sigaction(ID4SIG, &sa, NULL))
	{
		printf("Signal action bind failure: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Define signal event for timer
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = ID4SIG;
	sev.sigev_value.sival_ptr = &id4timerid;
	// Create timer
	if (timer_create(CLOCK_REALTIME, &sev, &id4timerid))
	{
		printf("Timer create failure: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Declare timer (1M ticks)
	ttLocalTime = time(NULL);
	localtime_r(&ttLocalTime, &tmLocalTime);

	its.it_value.tv_sec = 60 - tmLocalTime.tm_sec;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 60;
	its.it_interval.tv_nsec = 0;
	// Start timer
	if (timer_settime(id4timerid, 0, &its, NULL))
	{
		printf("Timer start failure: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (bWebEnable)
	{
		// Wait for webio to exit
		pthread_join(tWebIO, NULL);
	}

	// Cleanup timers, threads & queues
	timer_delete(id4timerid);
	pthread_cancel(tID4Clock);
	pthread_join(tID4Clock, NULL);

	mq_unlink(MQ_NAME);

	CloseSerPort(fPort);

	return 0;
}
