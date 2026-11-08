//
// response.c
//

#include "filter.h"

#include "response.h"

#include "qcfs.h"

#include <response.tmh>

VOID
InitializeResponse(_In_ PFILTER_CONTEXT Context, _In_ NTSTATUS Status, _In_ ULONG NeededSize)
{
    PQCFS_RESPONSE Response;

    if (Context == NULL || Context->OutputBuffer == NULL)
    {
        return;
    }

    RtlZeroMemory(Context->OutputBuffer, Context->OutputLength);

    Response = (PQCFS_RESPONSE)Context->OutputBuffer;
    Response->Ioctl = Context->Ioctl;
    Response->Status = Status;
    Response->NeededSize = NeededSize;
    Response->DataSize = 0;

    Trace(
        TRACE_LEVEL_VERBOSE,
        TRACE_DRIVER,
        "InitializeResponse Ioctl=0x%08X Status=0x%08X Needed=%lu",
        Context->Ioctl,
        Status,
        NeededSize);
}

NTSTATUS
BuildStaticFileResponse(_In_ PFILTER_CONTEXT Context, _In_reads_bytes_(DataSize) const VOID *Data, _In_ ULONG DataSize)
{
    PQCFS_RESPONSE Response;

    if (Context == NULL || Data == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Context->OutputLength < (296 + DataSize))
    {
        InitializeResponse(Context, STATUS_BUFFER_TOO_SMALL, DataSize);

        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_DRIVER,
            "Static response buffer too small Need=%lu Have=%lu",
            296 + DataSize,
            Context->OutputLength);

        return STATUS_SUCCESS;
    }

    InitializeResponse(Context, STATUS_SUCCESS, 0);

    Response = (PQCFS_RESPONSE)Context->OutputBuffer;
    Response->DataSize = DataSize;
    RtlCopyMemory(Response->Data, Data, DataSize);

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Built static response DataSize=%lu", DataSize);

    return STATUS_SUCCESS;
}

NTSTATUS
BuildFileSizeResponse(_In_ PFILTER_CONTEXT Context, _In_ ULONG FileSize)
{
    PQCFS_RESPONSE Response;

    if (Context == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Context->OutputLength < (296 + sizeof(ULONG)))
    {
        InitializeResponse(Context, STATUS_BUFFER_TOO_SMALL, sizeof(ULONG));

        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "FileSize response buffer too small");

        return STATUS_SUCCESS;
    }

    InitializeResponse(Context, STATUS_SUCCESS, 0);

    Response = (PQCFS_RESPONSE)Context->OutputBuffer;
    Response->DataSize = sizeof(ULONG);
    *(PULONG)Response->Data = FileSize;

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Returning file size %lu", FileSize);

    return STATUS_SUCCESS;
}

NTSTATUS
BuildDirectoryResponse(_In_ PFILTER_CONTEXT Context, _In_ ULONG NumberOfFiles)
{
    PQCFS_RESPONSE Response;
    ULONG RequiredSize;

    if (Context == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (NumberOfFiles > (MAXULONG / sizeof(QCFS_DIRECTORY_ENTRY)))
    {
        return STATUS_INTEGER_OVERFLOW;
    }

    RequiredSize = NumberOfFiles * sizeof(QCFS_DIRECTORY_ENTRY);

    if (Context->OutputLength < (296 + RequiredSize))
    {
        InitializeResponse(Context, STATUS_BUFFER_TOO_SMALL, RequiredSize);

        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_DRIVER,
            "Directory buffer too small Need=%lu Have=%lu",
            RequiredSize,
            Context->OutputLength);

        return STATUS_SUCCESS;
    }

    InitializeResponse(Context, STATUS_SUCCESS, 0);

    Response = (PQCFS_RESPONSE)Context->OutputBuffer;
    Response->DataSize = RequiredSize;

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_DRIVER,
        "Directory response Files=%lu DataSize=%lu",
        NumberOfFiles,
        RequiredSize);

    return STATUS_SUCCESS;
}

PVOID
GetResponsePayload(_In_ PFILTER_CONTEXT Context)
{
    if (Context == NULL || Context->OutputBuffer == NULL)
    {
        return NULL;
    }

    return ((PQCFS_RESPONSE)Context->OutputBuffer)->Data;
}

ULONG
GetResponsePayloadSize(_In_ PFILTER_CONTEXT Context)
{
    if (Context == NULL || Context->OutputBuffer == NULL)
    {
        return 0;
    }

    return ((PQCFS_RESPONSE)Context->OutputBuffer)->DataSize;
}

NTSTATUS
CommitResponse(_In_ PFILTER_CONTEXT Context)
{
    NTSTATUS status;

    if (Context == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Trace(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "CommitResponse %lu bytes", Context->OutputLength);

    status = WdfMemoryCopyFromBuffer(Context->OutputMemory, 0, Context->OutputBuffer, Context->OutputLength);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "CommitResponse failed 0x%08X", status);
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Response committed");

    return STATUS_SUCCESS;
}