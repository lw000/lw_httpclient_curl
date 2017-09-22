#ifndef __LIB_HTTPCLIENT_H__
#define __LIB_HTTPCLIENT_H__

#ifdef TRDAHTTPCLIENT_EXPORTS
#	ifdef __WIN32
#		define EXPORTS_API __declspec(dllexport)
#	else
#		define EXPORTS_API __declspec(dllimport)
#	endif
#else
#	define EXPORTS_API
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <string>

#endif // !__LIB_HTTPCLIENT_H__
