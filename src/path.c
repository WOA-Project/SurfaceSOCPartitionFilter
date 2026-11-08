//
// path.c
//

#include "filter.h"

#include "path.h"
#include "qcfs.h"

#include <path.tmh>

NTSTATUS
ExtractPath(_In_reads_bytes_(InputLength) PUCHAR InputBuffer, _In_ ULONG InputLength, _Out_ PPARSED_PATH ParsedPath)
{
    ULONG Remaining;
    ULONG CharacterCount;

    if (InputBuffer == NULL || ParsedPath == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(ParsedPath, sizeof(*ParsedPath));

    if (InputLength <= QCFS_PATH_OFFSET)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    Remaining = InputLength - QCFS_PATH_OFFSET;

    if ((Remaining % sizeof(WCHAR)) != 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    CharacterCount = Remaining / sizeof(WCHAR);

    if (CharacterCount > RTL_NUMBER_OF(ParsedPath->Buffer))
    {
        CharacterCount = RTL_NUMBER_OF(ParsedPath->Buffer);
    }

    RtlCopyMemory(ParsedPath->Buffer, InputBuffer + QCFS_PATH_OFFSET, CharacterCount * sizeof(WCHAR));

    ParsedPath->Buffer[RTL_NUMBER_OF(ParsedPath->Buffer) - 1] = UNICODE_NULL;

    RtlInitUnicodeString(&ParsedPath->Path, ParsedPath->Buffer);

    Trace(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Extracted Path='%wZ'", &ParsedPath->Path);

    return STATUS_SUCCESS;
}

BOOLEAN
ContainsParentTraversal(_In_ PCUNICODE_STRING Path)
{
    if (Path == NULL || Path->Buffer == NULL)
    {
        return FALSE;
    }

    return (wcsstr(Path->Buffer, L"..") != NULL);
}

BOOLEAN
ValidateJsonPath(_In_ PCUNICODE_STRING Path)
{
    UNICODE_STRING Prefix;

    if (Path == NULL || Path->Buffer == NULL)
    {
        return FALSE;
    }

    RtlInitUnicodeString(&Prefix, L"JSON\\");

    if (!RtlPrefixUnicodeString(&Prefix, Path, TRUE))
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "JSON validation failed (prefix): '%wZ'", Path);
        return FALSE;
    }

    if (Path->Length <= Prefix.Length)
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "JSON validation failed (empty filename): '%wZ'", Path);
        return FALSE;
    }

    if (ContainsParentTraversal(Path))
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "JSON validation failed (traversal): '%wZ'", Path);
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
IsJsonRoot(_In_ PCUNICODE_STRING Path)
{
    UNICODE_STRING JsonRoot;

    if (Path == NULL)
    {
        return FALSE;
    }

    RtlInitUnicodeString(&JsonRoot, L"JSON");

    return RtlEqualUnicodeString(Path, &JsonRoot, TRUE);
}

BOOLEAN
IsJsonFile(_In_ PCUNICODE_STRING Path)
{
    UNICODE_STRING Prefix;

    if (Path == NULL)
    {
        return FALSE;
    }

    RtlInitUnicodeString(&Prefix, L"JSON\\");

    return RtlPrefixUnicodeString(&Prefix, Path, TRUE);
}

BOOLEAN
PathEqualsLiteral(_In_ PCUNICODE_STRING Path, _In_ PCWSTR Literal)
{
    UNICODE_STRING Compare;

    if (Path == NULL || Literal == NULL)
    {
        return FALSE;
    }

    RtlInitUnicodeString(&Compare, Literal);

    return RtlEqualUnicodeString(Path, &Compare, TRUE);
}