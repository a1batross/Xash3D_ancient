//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        stdlib.c - std lib portable utils
//=======================================================================

#include "launcher.h"

char *strupper(char *start)
{
	char *in;
	in = start;
	while (*in)
	{
		*in = toupper(*in);
		in++;
	}
	return start;
}

size_t strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	// Find the end of dst and adjust bytes left but don't go past end
	while (n-- != 0 && *d != '\0') d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0) return(dlen + strlen(s));
	while (*s != '\0')
	{
		if (n != 1)
		{
			*d++ = *s;
			n--;
		}
		s++;
	}

	*d = '\0';
	return(dlen + (s - src));	//count does not include NUL
}

size_t strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	// Copy as many bytes as will fit
	if (n != 0 && --n != 0)
	{
		do
		{
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	// Not enough room in dst, add NUL and traverse rest of src
	if (n == 0)
	{
		if (siz != 0) *d = '\0'; //NUL-terminate dst
		while (*s++);
	}
	return(s - src - 1); //count does not include NUL
}

/*
====================
timestamp
====================
*/
const char* tstamp( int format )
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	time (&crt_time);
	crt_tm = localtime (&crt_time);
	switch( format )
	{
	case TIME_FULL:
		// Build the full timestamp (ex: "Apr03 2007 [23:31.55]");
		strftime(timestring, sizeof (timestring), "%b%d %Y [%H:%M.%S]", crt_tm);
		break;
	case TIME_DATE_ONLY:
		// Build the date stamp only (ex: "Apr03 2007");
		strftime(timestring, sizeof (timestring), "%b%d %Y", crt_tm);
		break;
	case TIME_TIME_ONLY:
		// Build the time stamp only (ex: "[23:31.55]");
		strftime(timestring, sizeof (timestring), "[%H:%M.%S]", crt_tm);
		break;
	case TIME_NO_SECONDS:
		// Build the full timestamp (ex: "Apr03 2007 [23:31]");
		strftime(timestring, sizeof (timestring), "%b%d %Y [%H:%M]", crt_tm);
		break;
	}

	strcpy( timestamp, timestring );
	return timestamp;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *_va(const char *format, ...)
{
	va_list argptr;
	static char string[8][1024], *s;
	static int stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 7;
	va_start (argptr, format);
	vsprintf (s, format, argptr);
	va_end (argptr);
	return s;
}


int vslprintf(char *buffer, size_t buffersize, const char *format, va_list args)
{
	int result;

	result = _vsnprintf (buffer, buffersize, format, args);
	if (result < 0 || (size_t)result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}

	return result;
}

int slprintf(char *buffer, size_t buffersize, const char *format, ...)
{
	va_list args;
	int result;

	va_start (args, format);
	result = vslprintf (buffer, buffersize, format, args);
	va_end (args);

	return result;
}