//
// filetable.h
//

#pragma once

#include <ntddk.h>

struct _FILTER_CONTEXT;

typedef struct _FILTER_CONTEXT FILTER_CONTEXT;
typedef FILTER_CONTEXT* PFILTER_CONTEXT;

typedef enum _FILE_MATCH_TYPE
{
    FileMatchExact,
    FileMatchPrefix
} FILE_MATCH_TYPE;

typedef enum _FILE_ENTRY_TYPE
{
    FileEntryStatic,
    FileEntryHandler
} FILE_ENTRY_TYPE;

typedef
NTSTATUS
(*FILE_HANDLER)(
    _In_ PFILTER_CONTEXT Context,
    _In_ PCUNICODE_STRING Path
    );

typedef struct _FILE_ENTRY
{
    FILE_MATCH_TYPE MatchType;
    PCWSTR Pattern;
    FILE_ENTRY_TYPE Type;
    union
    {
        struct
        {
            const VOID* Data;
            ULONG Size;
        } StaticFile;
        FILE_HANDLER Handler;
    };
} FILE_ENTRY;

extern const FILE_ENTRY g_FileTable[];

extern const ULONG g_FileTableCount;