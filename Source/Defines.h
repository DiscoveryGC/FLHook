#pragma once


// Current version of FLHook.
#define VERSION L"3.1"


// Maximum limit of players.
// Setting it to greater value may lead to bugs?
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


// Properties in C++
#define Property(GET, SET) _declspec(property(get = GET, put = SET))
#define PropertyReadonly(GET) _declspec(property(get = GET))


// Foreach for C++ from old rusty times. Would be great to get rid of this.
#define foreach(lst, type, var) for(list<type>::iterator var = lst.begin(); (var != lst.end()); var++)


// Who actually writes functions whose names coincide with std ones?
using namespace std;


// The namespace should contain only other namespaces which can be optionally used.
namespace FLHook
{
	// All solution-wide global variables must be here.
	namespace $Globals
	{

	};
};

using namespace FLHook;
using namespace FLHook::$Globals;
