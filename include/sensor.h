//
// sensor.h
//

#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "filter_context.h"

NTSTATUS
BuildSensorDataPath(
    _In_ PCUNICODE_STRING JsonPath,
    _Out_ PUNICODE_STRING SensorPath,
    _Out_writes_(BufferChars) PWCHAR Buffer,
    _In_ ULONG BufferChars
);

NTSTATUS
HandleSensorJson(
    _In_ PFILTER_CONTEXT Context,
    _In_ PCUNICODE_STRING Path
);