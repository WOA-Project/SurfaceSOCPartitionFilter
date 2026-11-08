//
// fileproperty.c
//

#include "filter.h"

#include "fileproperty.h"

#include "request.h"
#include "response.h"
#include "path.h"
#include "lookup.h"
#include "sensor.h"

#include "filetable.h"
#include "qcfs.h"
#include "sfpd.h"

#include "constants.h"

#include <fileproperty.tmh>

static NTSTATUS
GetDynamicFileSize(_In_ PFILTER_CONTEXT Context, _In_ PCUNICODE_STRING Path, _Out_ PULONG FileSize)
{
    NTSTATUS status;

    if (Context == NULL || Path == NULL || FileSize == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // BT.PROVISION
    //

    if (PathEqualsLiteral(Path, L"QCOM\\BT.PROVISION"))
    {
        *FileSize = sizeof(BT_PROVISION);
        return STATUS_SUCCESS;
    }

    //
    // WLAN.PROVISION
    //

    if (PathEqualsLiteral(Path, L"QCOM\\WLAN.PROVISION"))
    {
        *FileSize = sizeof(WLAN_PROVISION);
        return STATUS_SUCCESS;
    }

    //
    // WLAN_PMICXO.PROVISION
    //

    if (PathEqualsLiteral(Path, L"QCOM\\WLAN_PMICXO.PROVISION"))
    {
        return STATUS_FILE_NOT_AVAILABLE;
    }

    //
    // JSON\*
    //

    if (IsJsonFile(Path))
    {
        WCHAR SensorPathBuffer[MAX_PATH];
        UNICODE_STRING SensorPath;

        status = BuildSensorDataPath(Path, &SensorPath, SensorPathBuffer, RTL_NUMBER_OF(SensorPathBuffer));
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        status = GetSFPDItemSize(Context->Device, SensorPath.Buffer, FileSize);

        return status;
    }

    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS
GetStaticFileSize(_In_ PCUNICODE_STRING Path, _Out_ PULONG FileSize)
{
    const FILE_ENTRY *Entry;

    if (Path == NULL || FileSize == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Entry = FindFileEntry(Path);

    if (Entry == NULL)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (Entry->Type != FileEntryStatic)
    {
        return STATUS_NOT_SUPPORTED;
    }

    *FileSize = Entry->StaticFile.Size;

    return STATUS_SUCCESS;
}

NTSTATUS
HandleFileProperty(_In_ PFILTER_CONTEXT Context)
{
    NTSTATUS status;
    PARSED_PATH ParsedPath;
    ULONG Property;
    ULONG FileSize;

    status = GetInputPath(Context, &ParsedPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = GetInputUlong(Context, QCFS_FILE_PROPERTY_OFFSET, &Property);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "GetFileProperty Path='%wZ' Property=%lu", &ParsedPath.Path, Property);

    //
    // Original driver only supports:
    //
    // Property == 2
    //

    if (Property != QCFS_FILE_PROPERTY_SIZE)
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Unsupported file property %lu", Property);
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Static files
    //

    status = GetStaticFileSize(&ParsedPath.Path, &FileSize);
    if (NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Static file size=%lu", FileSize);
        return BuildFileSizeResponse(Context, FileSize);
    }

    //
    // Dynamic handlers
    //

    status = GetDynamicFileSize(Context, &ParsedPath.Path, &FileSize);

    if (status == STATUS_FILE_NOT_AVAILABLE)
    {
        PQCFS_RESPONSE Response;
        InitializeResponse(Context, STATUS_FILE_NOT_AVAILABLE, 0);

        Response = (PQCFS_RESPONSE)Context->OutputBuffer;
        Response->DataSize = sizeof(UCHAR);
        Response->Data[0] = 0;

        return STATUS_SUCCESS;
    }

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Dynamic file size=%lu", FileSize);

    return BuildFileSizeResponse(Context, FileSize);
}