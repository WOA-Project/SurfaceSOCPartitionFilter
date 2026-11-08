//
// sensor.c
//

#include "filter.h"

#include "sensor.h"
#include "path.h"
#include "response.h"

#include "qcfs.h"
#include "sfpd.h"

#include <sensor.tmh>

NTSTATUS
BuildSensorDataPath(PCUNICODE_STRING JsonPath, PUNICODE_STRING SensorPath, PWCHAR Buffer, ULONG BufferChars)
{
    UNREFERENCED_PARAMETER(BufferChars);

    if (JsonPath == NULL || SensorPath == NULL || Buffer == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (JsonPath->Length <= (5 * sizeof(WCHAR)))
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Buffer, 260 * sizeof(WCHAR));

    //
    // Start with:
    // \sensors\
    //

    RtlCopyMemory(Buffer, SENSOR_DATA_DIRECTORY, sizeof(SENSOR_DATA_DIRECTORY));

    //
    // Append:
    // foo.json
    // from JSON\foo.json
    //

    RtlCopyMemory(
        Buffer + ((sizeof(SENSOR_DATA_DIRECTORY) - sizeof(WCHAR)) / sizeof(WCHAR)),
        JsonPath->Buffer + 5,
        JsonPath->Length - (5 * sizeof(WCHAR)));

    Buffer[259] = UNICODE_NULL;

    RtlInitUnicodeString(SensorPath, Buffer);

    return STATUS_SUCCESS;
}

NTSTATUS
HandleSensorJson(_In_ PFILTER_CONTEXT Context, _In_ PCUNICODE_STRING Path)
{
    NTSTATUS status;
    WCHAR SensorPathBuffer[MAX_PATH];
    UNICODE_STRING SensorPath;
    ULONG ItemSize;
    PQCFS_RESPONSE Response;

    if (Context == NULL || Path == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!ValidateJsonPath(Path))
    {
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Invalid JSON path '%wZ'", Path);
        return STATUS_ACCESS_DENIED;
    }

    status = BuildSensorDataPath(Path, &SensorPath, SensorPathBuffer, RTL_NUMBER_OF(SensorPathBuffer));
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "BuildSensorDataPath failed 0x%08X", status);
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "JSON '%wZ' -> '%wZ'", Path, &SensorPath);

    status = GetSFPDItemSize(Context->Device, SensorPath.Buffer, &ItemSize);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "GetSFPDItemSize(%ws) failed 0x%08X", SensorPath.Buffer, status);
        return status;
    }

    Trace(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Sensor file size=%lu", ItemSize);

    if (Context->OutputLength < (296 + ItemSize))
    {
        InitializeResponse(Context, STATUS_BUFFER_TOO_SMALL, ItemSize);

        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_DRIVER,
            "Output buffer too small Need=%lu Have=%lu",
            296 + ItemSize,
            Context->OutputLength);

        return STATUS_SUCCESS;
    }

    InitializeResponse(Context, STATUS_SUCCESS, 0);
    Response = (PQCFS_RESPONSE)Context->OutputBuffer;
    Response->DataSize = ItemSize;

    status = GetSFPDItem(Context->Device, SensorPath.Buffer, Response->Data, ItemSize);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "GetSFPDItem(%ws) failed 0x%08X", SensorPath.Buffer, status);
        return status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Returned sensor data %lu bytes", ItemSize);

    return STATUS_SUCCESS;
}