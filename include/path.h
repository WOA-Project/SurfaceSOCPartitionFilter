//
// path.h
//

#pragma once

#include <ntddk.h>

typedef struct _PARSED_PATH
{
    UNICODE_STRING Path;
    WCHAR Buffer[96];
} PARSED_PATH, *PPARSED_PATH;

NTSTATUS
ExtractPath(
    _In_reads_bytes_(InputLength)
    PUCHAR InputBuffer,
    _In_ ULONG InputLength,
    _Out_ PPARSED_PATH ParsedPath
);

BOOLEAN
ContainsParentTraversal(
    _In_ PCUNICODE_STRING Path
);

BOOLEAN
ValidateJsonPath(
    _In_ PCUNICODE_STRING Path
);

BOOLEAN
IsJsonRoot(
    _In_ PCUNICODE_STRING Path
);

BOOLEAN
IsJsonFile(
    _In_ PCUNICODE_STRING Path
);

BOOLEAN
PathEqualsLiteral(
    _In_ PCUNICODE_STRING Path,
    _In_ PCWSTR Literal
);