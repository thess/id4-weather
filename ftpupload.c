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
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define REMOTE_URL      "ftp://dp1/"

int ftpUpload(char *srcFile, char *dstFile)
{
	CURL *curl;
	CURLcode res;
	FILE *hd_src;
	char sFTPTarget[32];

	// Open source
	hd_src = fopen(srcFile, "r");
	if (hd_src == NULL)
	{
		printf("FTP: Source '%s' missing: %s\n", srcFile, strerror(errno));
		return -1;
	}

	// Form output target string
	strcpy(sFTPTarget, REMOTE_URL);
	strcat(sFTPTarget, dstFile);

	// Init cURL
	curl_global_init(CURL_GLOBAL_ALL);

	// get a curl handle
	curl = curl_easy_init();

	if(curl)
	{
		// enable uploading
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

		// specify target
		curl_easy_setopt(curl, CURLOPT_URL, sFTPTarget);

		// specify user & pwd
		curl_easy_setopt(curl, CURLOPT_USERPWD, "weather:id4001-5");

		// now specify which file to upload
		curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

		// Perform upload
		res = curl_easy_perform(curl);
		// Check for errors
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		// always cleanup
		curl_easy_cleanup(curl);
	}

	// Close local file and cleanup
	fclose(hd_src);

	curl_global_cleanup();

	return 0;
}
