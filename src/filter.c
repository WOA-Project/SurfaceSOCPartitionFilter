//
// filter.c
//

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <ntdddisk.h>
#include <windef.h>
#include <wdfdriver.h>
#include <ntstrsafe.h>

#include "filter.h"

#include <filter.tmh>

#include "request.h"
#include "dispatch.h"
#include "response.h"

#include "request_context.h"

#ifdef ALLOC_PRAGMA
#    pragma alloc_text(INIT, DriverEntry)
#    pragma alloc_text(PAGE, OnDeviceAdd)
#    pragma alloc_text(PAGE, OnContextCleanup)
#endif

NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_DRIVER_CONFIG Config;
    WDFDRIVER Driver;
    NTSTATUS Status;

    WPP_INIT_TRACING(DriverObject, RegistryPath);

    WDF_DRIVER_CONFIG_INIT(&Config, OnDeviceAdd);

    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.EvtCleanupCallback = OnContextCleanup;

    Status = WdfDriverCreate(DriverObject, RegistryPath, &Attributes, &Config, &Driver);
    if (!NT_SUCCESS(Status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_INIT, "WdfDriverCreate failed 0x%08X", Status);
        WPP_CLEANUP(DriverObject);
    }

    return Status;
}

NTSTATUS
OnDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
    WDFDEVICE Device;
    WDF_IO_QUEUE_CONFIG QueueConfig;
    WDF_OBJECT_ATTRIBUTES Attributes;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attributes, REQUEST_CONTEXT);
    WdfDeviceInitSetRequestAttributes(DeviceInit, &Attributes);

    Status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &Device);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchSequential);
    QueueConfig.EvtIoDeviceControl = OnIoDeviceControl;

    Status = WdfIoQueueCreate(Device, &QueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);

    return Status;
}

VOID
OnIoDeviceControl(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode)
{
    WDFDEVICE Device;
    WDFIOTARGET Target;

    WDFMEMORY InputMemory = NULL;
    WDFMEMORY OutputMemory = NULL;

    WDF_REQUEST_SEND_OPTIONS Options;

    NTSTATUS Status;

    BOOLEAN Forward = FALSE;

    Device = WdfIoQueueGetDevice(Queue);
    Target = WdfDeviceGetIoTarget(Device);

    switch (IoControlCode)
    {
    case IOCTL_QCFS_READ_FILE:
    case IOCTL_QCFS_GET_FILE_PROPERTY:
    case IOCTL_QCFS_LIST_DIRECTORY_FILES:
        Forward = TRUE;
        break;
    default:
        break;
    }

    if (!Forward)
    {
        WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

        if (!WdfRequestSend(Request, Target, &Options))
        {
            Status = WdfRequestGetStatus(Request);
            WdfRequestComplete(Request, Status);
        }

        return;
    }

    if (InputBufferLength)
    {
        Status = WdfRequestRetrieveInputMemory(Request, &InputMemory);
        if (!NT_SUCCESS(Status))
        {
            WdfRequestComplete(Request, Status);
            return;
        }
    }

    if (OutputBufferLength)
    {
        Status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
        if (!NT_SUCCESS(Status))
        {
            WdfRequestComplete(Request, Status);
            return;
        }
    }

    Status = WdfIoTargetFormatRequestForIoctl(Target, Request, IoControlCode, InputMemory, NULL, OutputMemory, NULL);
    if (!NT_SUCCESS(Status))
    {
        WdfRequestComplete(Request, Status);
        return;
    }

    {
        REQUEST_CONTEXT *Context;
        Context = GetRequestContext(Request);
        Context->InputBufferLength = (ULONG)InputBufferLength;

        WdfRequestSetCompletionRoutine(Request, OnRequestCompletionRoutine, Context);
    }

    if (!WdfRequestSend(Request, Target, WDF_NO_SEND_OPTIONS))
    {
        Status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, Status);
    }
}

VOID
OnRequestCompletionRoutine(
    WDFREQUEST Request,
    WDFIOTARGET Target,
    PWDF_REQUEST_COMPLETION_PARAMS Params,
    WDFCONTEXT RequestContext)
{
    FILTER_CONTEXT FilterContext;
    NTSTATUS Status;

    RtlZeroMemory(&FilterContext, sizeof(FilterContext));

    Status = FilterContextInitialize(Request, Target, Params, RequestContext, &FilterContext);
    if (!NT_SUCCESS(Status))
    {
        goto cleanup;
    }

    Status = DispatchVirtualIoctl(&FilterContext);
    if (!NT_SUCCESS(Status))
    {
        goto cleanup;
    }

    Status = CommitResponse(&FilterContext);

cleanup:

    FilterContextDestroy(&FilterContext);
    WdfRequestComplete(Request, Status);
}

VOID
OnContextCleanup(WDFOBJECT Driver)
{
    PAGED_CODE();
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));
}