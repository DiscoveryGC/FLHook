#pragma once


// Current version of FLHook.
#define VERSION L"3.1"


// Maximum players online.
#define MAX_CLIENT_ID 249


#define EXTENDED_EXCEPTION_LOGGING


// Extra types
typedef unsigned char byte;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long int bigint;
typedef unsigned long long int biguint;
typedef unsigned __int64 mstime;


// Import - export
#define IMPORT __declspec(dllimport)
#define EXPORT __declspec(dllexport)


// Labelled loops in C++
#define named(loop_name) goto loop_name; \
                         loop_name##_skip: if (0) \
                         loop_name:

#define _ALLOW_KEYWORD_MACROS
#define break(loop_name) goto loop_name##_skip


// Foreach for C++ from old rusty times. Would be great to get rid of this.
#define foreach(lst, type, var) for(list<type>::iterator var = lst.begin(); (var != lst.end()); var++)


// Who actually names own stuff to be ambiguous with std?
using namespace std;


// The namespace contains only other namespaces which you can optionally use.
namespace FLHook
{
	// All solution-wide global variables must be here, somewhen in future.
	namespace $Globals
	{

	};
};

using namespace FLHook;
using namespace FLHook::$Globals;
