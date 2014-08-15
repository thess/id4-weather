/* wsfcode.c
 *
 * This file was created by an automated utility.
 * It is not intended for manual editing
 */

#include "webio/websys.h"
#include "webio/webio.h"
#include "webio/webfs.h"
#include "wsfdata.h"

#include "ID4Serial.h"
#include "serport.h"

extern struct tm tmLocalTime;
extern char *sWinDir[];
extern int fPort;

int
wi_cvariables(wi_sess * sess, int token)
{
	int	nPres1, nPres2, nWinDir;
	unsigned char *sWBuf;
	int	e = 0;

	switch(token)
	{
	case LOCALTIME_VAR4:
		e = wi_putstring(sess, wi_getdate(sess, (struct tm *)&tmLocalTime));
		break;

	case WCURRENT_VAR5:
		sWBuf = malloc(WEATHER_BUF_SIZE);
		if (sWBuf)
		{
			if (ReadWeather(sWBuf) == 0)
			{
				// Convert presure
				nPres1 = (sWBuf[11] + 2900) / 100;
				nPres2 = (sWBuf[11] + 2900) - (nPres1 * 100);

				// Fold input 4-bit gray code to table index (wind direction)
				nWinDir = (sWBuf[1] & 0x1C) >> 2;
				nWinDir |= (sWBuf[1] & 0x80) >> 4;

				wi_printf(sess, "Indoor: %d, Outdoor: %d, Wind direction: %s, Speed: %d, Pressure: %d.%02d",
						  sWBuf[9] - 40, sWBuf[10] - 40,
						  sWinDir[nWinDir], (sWBuf[8] * 99) / 256,
						  nPres1, nPres2);
			}
			else
			{
				wi_printf(sess, "-- No weather available --");
			}
			free(sWBuf);
		}
		else
		{
			wi_printf(sess, "-- Out of memory --");
		}
		break;

	default:
		wi_printf(sess, "<Undefined variable>");
		e = -1;
		break;
	}

	return e;
}

