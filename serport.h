// serport.h

#ifndef SERPORT_H_INCLUDED
#define SERPORT_H_INCLUDED

int OpenSerPort (char *sDeviceName);
int WriteSerPort(int fd, unsigned char *psOutput, int nCount);
int ReadSerPort(int fd, unsigned char *psResponse, int iMax, unsigned char cCmd);
void CloseSerPort(int fd);

#define RESP_HDR "const unsigned char %c_response[%d] = {\n    "

#define WLOG_PATH   "/opt/weather"

#if defined(RECORD_MODE)
extern FILE *fLog;
extern int bRecording;
#endif

#endif // SERPORT_H_INCLUDED
