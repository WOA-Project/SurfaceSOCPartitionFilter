//
// lookup.c
//

#include "filter.h"

#include "lookup.h"

#include "filetable.h"
#include "response.h"

#include <lookup.tmh>

BOOLEAN
EntryMatches(_In_ const FILE_ENTRY *Entry, _In_ PCUNICODE_STRING Path)
{
    UNICODE_STRING Pattern;

    if (Entry == NULL || Path == NULL)
    {
        return FALSE;
    }

    RtlInitUnicodeString(&Pattern, Entry->Pattern);

    switch (Entry->MatchType)
    {
    case FileMatchExact:
        return RtlEqualUnicodeString(Path, &Pattern, TRUE);
    case FileMatchPrefix:
        return RtlPrefixUnicodeString(&Pattern, Path, TRUE);
    default:
        return FALSE;
    }
}

const FILE_ENTRY *
FindFileEntry(_In_ PCUNICODE_STRING Path)
{
    ULONG i;

    if (Path == NULL)
    {
        return NULL;
    }

    for (i = 0; i < g_FileTableCount; i++)
    {
        if (EntryMatches(&g_FileTable[i], Path))
        {
            return &g_FileTable[i];
        }
    }

    return NULL;
}

NTSTATUS
HandleFileLookup(_In_ PFILTER_CONTEXT Context, _In_ PCUNICODE_STRING Path)
{
    const FILE_ENTRY *Entry;

    if (Context == NULL || Path == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Entry = FindFileEntry(Path);
    if (Entry == NULL)
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "No file mapping for '%wZ'", Path);
        return STATUS_NOT_SUPPORTED;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Matched '%ws'", Entry->Pattern);

    switch (Entry->Type)
    {
    case FileEntryStatic:
        return BuildStaticFileResponse(Context, Entry->StaticFile.Data, Entry->StaticFile.Size);
    case FileEntryHandler:
        return Entry->Handler(Context, Path);
    default:
        return STATUS_NOT_SUPPORTED;
    }
}