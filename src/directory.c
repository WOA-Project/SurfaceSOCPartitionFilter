//
// directory.c
//

#include "filter.h"

#include "directory.h"

#include "request.h"
#include "response.h"
#include "path.h"

#include "qcfs.h"
#include "sfpd.h"

#include <directory.tmh>

NTSTATUS
HandleDirectoryListing(_In_ PFILTER_CONTEXT Context)
{
    NTSTATUS status;
    PARSED_PATH ParsedPath;
    ULONG Property;
    ULONG NumberOfFiles;
    PQCFS_RESPONSE Response;

    if (Context == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    status = GetInputPath(Context, &ParsedPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = GetInputUlong(Context, QCFS_DIRECTORY_PROPERTY_OFFSET, &Property);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "ListDirectory Path='%wZ' Property=%lu", &ParsedPath.Path, Property);

    //
    // Original implementation only supports:
    //
    // Property == 10
    //
    if (Property != QCFS_DIRECTORY_PROPERTY_FILES)
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Unsupported directory property %lu", Property);
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Original implementation only exposes:
    //
    // JSON
    //
    if (!IsJsonRoot(&ParsedPath.Path))
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Unsupported directory '%wZ'", &ParsedPath.Path);
        return STATUS_NOT_SUPPORTED;
    }

    status = GetSFPDNumberOfFilesInDirectory(Context->Device, SENSOR_DATA_DIRECTORY, &NumberOfFiles);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "GetSFPDNumberOfFilesInDirectory failed 0x%08X", status);
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Found %lu sensor files", NumberOfFiles);

    status = BuildDirectoryResponse(Context, NumberOfFiles);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    Response = (PQCFS_RESPONSE)Context->OutputBuffer;

    //
    // BuildDirectoryResponse returns STATUS_SUCCESS
    // even when STATUS_BUFFER_TOO_SMALL is encoded
    // inside the protocol response.
    //

    if (Response->Status == STATUS_BUFFER_TOO_SMALL)
    {
        return STATUS_SUCCESS;
    }

    status = GetSFPDFilesInDirectory(Context->Device, SENSOR_DATA_DIRECTORY, NumberOfFiles, Response->Data);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "GetSFPDFilesInDirectory failed 0x%08X", status);
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Directory enumeration completed (%lu files)", NumberOfFiles);
    return STATUS_SUCCESS;
}