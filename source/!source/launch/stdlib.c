//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        stdlib.c - std lib portable utils
//=======================================================================

#include "launcher.h"

void strupper(const char *in, char *out, size_t size_out)
{
	if (size_out == 0) return;

	while (*in && size_out > 1)
	{
		if (*in >= 'a' && *in <= 'z')
			*out++ = *in++ + 'A' - 'a';
		else *out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

void strlower(const char *in, char *out, size_t size_out)
{
	if (size_out == 0) return;

	while (*in && size_out > 1)
	{
		if (*in >= 'A' && *in <= 'Z')
			*out++ = *in++ + 'a' - 'A';
		else *out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

/*
============
strlength

returned string length
============
*/
int strlength( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		p++;
		len++;
	}
	return len;
}

/*
============
qstrlength

skipped color prefixes
============
*/
int qstrlength( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		if(IsColorString( p ))
		{
			p += 2;
			continue;
		}
		p++;
		len++;
	}
	return len;
}

char caseupper(const char in )
{
	char out;

	if (in >= 'a' && in <= 'z')
		out = in + 'A' - 'a';
	else out = in;

	return out;
}

char caselower(const char in )
{
	char out;

	if (in >= 'A' && in <= 'Z')
		out = in + 'a' - 'A';
	else out = in;

	return out;
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

	if (n == 0) return(dlen + strlength(s));
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

	// Not enough room in dst, add NULL and traverse rest of src
	if (n == 0)
	{
		if (siz != 0) *d = '\0'; //NULL-terminate dst
		while (*s++);
	}
	return(s - src - 1); // count does not include NULL
}

int strtoint(const char *str)
{
	int       val = 0;
	int	c, sign;

	if( !str ) return 0;
	
	if(*str == '-')
	{
		sign = -1;
		str++;
	}
	else sign = 1;
		
	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while(1)
		{
			c = *str++;
			if (c >= '0' && c <= '9') val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f') val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') val = (val<<4) + c - 'A' + 10;
			else return val * sign;
		}
	}
	
	// check for character
	if (str[0] == '\'') return sign * str[1];
	
	// assume decimal
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val * sign;
		val = val*10 + c - '0';
	}
	return 0;
}

float strtofloat(const char *str)
{
	double	val = 0;
	int	c, sign, decimal, total;

	if( !str ) return 0.0f;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else sign = 1;
		
	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9') val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f') val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') val = (val * 16) + c - 'A' + 10;
			else return val * sign;
		}
	}
	
	// check for character
	if (str[0] == '\'') return sign * str[1];
	
	// assume decimal
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9') break;
		val = val*10 + c - '0';
		total++;
	}

	if(decimal == -1) return val * sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}
	
	return val * sign;
}

void strtovec( float *vec, const char *str )
{
	char	*pstr, *pfront, buffer[MAX_QPATH];
	int	j;

	strlcpy( buffer, str, MAX_QPATH );
	pstr = pfront = buffer;

	for ( j = 0; j < 3; j++ )
	{
		vec[j] = strtofloat( pfront );

		// valid separator is space or ,
		while( *pstr && (*pstr != ' ' || *pstr != ',' ))
			pstr++;

		if (!*pstr) break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2) memset( vec, 0, sizeof(vec3_t)); 
}


/*
============
strlchr

find one charcster in string
============
*/
char *strlchr(const char *s, char c)
{
	int	len = strlength(s);
	s += len;
	while(len--) if(*--s == c) return (char *)s;
	return 0;
}

int strlcasecmp(const char *s1, const char *s2, int n)
{
	int             c1, c2;
	
	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) return 0; // strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z') c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z') c2 -= ('a' - 'A');
			if (c1 != c2) return -1; // strings not equal
		}
		if (!c1) return 0; // strings are equal
	}
	return -1;
}

int strlcmp (const char *s1, const char *s2, int n)
{
	while (1)
	{
		if (!n--) return 0;
		if (*s1 != *s2) return -1; // strings not equal    
		if (!*s1) return 0; // strings are equal
		s1++, s2++;
	}
	return -1;
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
strcasestr

search case - insensitive for string2 in string
============
*/
char *strcasestr( const char *string, const char *string2 )
{
	int c, len;

	if (!string || !string2) return NULL;

	c = caselower( *string2 );
	len = strlength( string2 );

	while (string)
	{
		for ( ; *string && caselower( *string ) != c; string++ );
		if (*string)
		{
			if(!strlcasecmp( string, string2, len ))
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
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
	vslprintf (s, sizeof (string[0]), format, argptr);
	va_end (argptr);
	return s;
}