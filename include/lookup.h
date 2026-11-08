//
// lookup.h
//

#pragma once

#include "filter_context.h"
#include "filetable.h"

BOOLEAN
EntryMatches(
    _In_ const FILE_ENTRY* Entry,
    _In_ PCUNICODE_STRING Path
);

const FILE_ENTRY*
FindFileEntry(
    _In_ PCUNICODE_STRING Path
);

NTSTATUS
HandleFileLookup(
    _In_ PFILTER_CONTEXT Context,
    _In_ PCUNICODE_STRING Path
);