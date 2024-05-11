// Copyright (c) Microsoft Corporation. All Rights Reserved.
// Copyright (c) DuoWoA authors. All Rights Reserved.


#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <ntdddisk.h>
#include <windef.h>
#include <wdfdriver.h>
#include <ntstrsafe.h>

#include "filter.h"
#include <filter.tmh>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, OnDeviceAdd)
#pragma alloc_text (PAGE, OnInternalDeviceControl)
#pragma alloc_text (PAGE, OnContextCleanup)
#pragma alloc_text (PAGE, OnRequestCompletionRoutine)
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object
    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful, error code otherwise.

--*/
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDFDRIVER hDriver;

    //
    // Initialize tracing via WPP
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    //
    // Create a framework driver object
    //
    WDF_DRIVER_CONFIG_INIT(&config, OnDeviceAdd);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = OnContextCleanup;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        &hDriver
    );

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_INIT,
            "Error creating WDF driver object - 0x%08lX",
            status);

        WPP_CLEANUP(DriverObject);

        goto exit;
    }

exit:

    return status;
}

NTSTATUS
OnDeviceAdd(
    IN WDFDRIVER Driver,
    IN PWDFDEVICE_INIT DeviceInit
)
/*++

Routine Description:

    OnDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager when a device is found. We create and
    initialize a WDF device object to represent the new instance of
    an touch device. Per-device objects are also instantiated.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry
    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS indicating success or failure

--*/
{
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    //
    // Tell the framework that you are filter driver. Framework
    // takes care of inherting all the device flags & characterstics
    // from the lower device you are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);

    status = WdfDeviceCreate(
        &DeviceInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &device);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_INIT,
            "WdfDeviceCreate failed - 0x%08lX",
            status);

        goto exit;
    }

    //
    // Create a parallel dispatch queue to handle requests from HID Class
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchSequential);

    queueConfig.EvtIoDeviceControl = OnIoDeviceControl;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE); // pointer to default queue

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_INIT,
            "Error creating WDF default queue - 0x%08lX",
            status);

        goto exit;
    }

exit:

    return status;
}

VOID OnIoDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
)
{
    NTSTATUS status;
    WDFDEVICE device;
    BOOLEAN forwardWithCompletionRoutine = FALSE;
    BOOLEAN requestSent = TRUE;
    WDF_REQUEST_SEND_OPTIONS options;
    WDFMEMORY inputMemory = NULL;
    WDFMEMORY outputMemory = NULL;
    WDFIOTARGET Target;

    PAGED_CODE();

    device = WdfIoQueueGetDevice(Queue);
    Target = WdfDeviceGetIoTarget(device);

    Trace(
        TRACE_LEVEL_ERROR,
        TRACE_INIT,
        "OnIoDeviceControl: OutputBufferLength: %d, InputBufferLength: %d, IoControlCode: %d\n",
        (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);

    //
    // Please note that QCSOCPartition provides the buffer in the Irp->UserBuffer
    // field irrespective of the ioctl buffer type. However, framework is very
    // strict about type checking. You cannot get Irp->UserBuffer by using
    // WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
    // internal ioctl. So depending on the ioctl code, we will either
    // use retreive function or escape to WDM to get the UserBuffer.
    //

    //switch (IoControlCode) {
    //case 0:
        //
        // Obtains the report descriptor for the HID device
        //
        forwardWithCompletionRoutine = TRUE;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_INIT,
            "OnIoDeviceControl: forwardWithCompletionRoutine = TRUE!\n");
    //    break;
    //
    //default:
    //    break;
    //}

    //
    // Forward the request down. WdfDeviceGetIoTarget returns
    // the default target, which represents the device attached to us below in
    // the stack.
    //
    if (forwardWithCompletionRoutine) {
        if (InputBufferLength > 0)
        {
            //
            // Format the request with the input memory so the completion routine
            // can access the input data in order to cache it into the context area
            //
            status = WdfRequestRetrieveInputMemory(Request, &inputMemory);

            if (!NT_SUCCESS(status)) {
                Trace(
                    TRACE_LEVEL_ERROR,
                    TRACE_INIT,
                    "WdfRequestRetrieveInputMemory failed: 0x%x\n",
                    status);

                WdfRequestComplete(Request, status);
                return;
            }
        }

        if (OutputBufferLength > 0)
        {
            //
            // Format the request with the output memory so the completion routine
            // can access the return data in order to cache it into the context area
            //
            status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);

            if (!NT_SUCCESS(status)) {
                Trace(
                    TRACE_LEVEL_ERROR,
                    TRACE_INIT,
                    "WdfRequestRetrieveOutputMemory failed: 0x%x\n",
                    status);

                WdfRequestComplete(Request, status);
                return;
            }
        }

        status = WdfIoTargetFormatRequestForIoctl(
            Target,
            Request,
            IoControlCode,
            inputMemory,
            NULL,
            outputMemory,
            NULL);

        if (!NT_SUCCESS(status)) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "WdfIoTargetFormatRequestForIoctl failed: 0x%x\n",
                status);

            WdfRequestComplete(Request, status);
            return;
        }

        //
        // Set our completion routine with a context area that we will save
        // the output data into
        //
        WdfRequestSetCompletionRoutine(
            Request,
            OnRequestCompletionRoutine,
            &InputBufferLength);

        requestSent = WdfRequestSend(
            Request,
            Target,
            WDF_NO_SEND_OPTIONS);
    }
    else
    {
        //
        // We are not interested in post processing the IRP so
        // fire and forget.
        //
        WDF_REQUEST_SEND_OPTIONS_INIT(
            &options,
            WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

        requestSent = WdfRequestSend(Request, Target, &options);
    }

    if (requestSent == FALSE) {
        status = WdfRequestGetStatus(Request);

        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_INIT,
            "WdfRequestSend failed: 0x%x\n",
            status);

        WdfRequestComplete(Request, status);
    }

    return;
}

VOID
OnRequestCompletionRoutine (
    IN WDFREQUEST  Request,
    IN WDFIOTARGET  Target,
    IN PWDF_REQUEST_COMPLETION_PARAMS  Params,
    IN WDFCONTEXT  Context
    )
/*++

Routine Description:

    Completion Routine

Arguments:

    Target - Target handle
    Request - Request handle
    Params - request completion params
    Context - Driver supplied context


Return Value:

    VOID

--*/
{
    WDFMEMORY   inputMemory = Params->Parameters.Ioctl.Input.Buffer;
    WDFMEMORY   outputMemory = Params->Parameters.Ioctl.Output.Buffer;
    NTSTATUS    status = Params->IoStatus.Status;

    if (Context == NULL)
    {
        goto exit;
    }

    ULONG inputBufferLength = *(PULONG)Context;
    ULONG outputBufferLength = (ULONG)Params->Parameters.Ioctl.Output.Length;

    PUCHAR inputBuffer = NULL;
    PUCHAR outputBuffer = NULL;

    UNREFERENCED_PARAMETER(Target);

    Trace(
        TRACE_LEVEL_ERROR,
        TRACE_INIT,
        "OnRequestCompletionRoutine: IoControlCode: %d - Status: %d\n",
        Params->Parameters.Ioctl.IoControlCode,
        Params->IoStatus.Status);

    if (inputBufferLength > 0)
    {
        inputBuffer = (PUCHAR)ExAllocatePoolWithTag(
            NonPagedPoolNx,
            inputBufferLength,
            HID_DESCRIPTOR_POOL_TAG);

        if (NULL == inputBuffer) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "Could not allocate input buffer!");

            status = STATUS_UNSUCCESSFUL;
            goto exit;
        }

        status = WdfMemoryCopyToBuffer(
            inputMemory,
            Params->Parameters.Ioctl.Input.Offset,
            inputBuffer,
            inputBufferLength);

        if (!NT_SUCCESS(status)) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "WdfMemoryCopyToBuffer failed: 0x%x\n",
                status);

            status = STATUS_UNSUCCESSFUL;
            goto free_input_buffer;
        }

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SOCPARTITION(%d): INPUT: LENGTH=%d", Params->Parameters.Ioctl.IoControlCode, inputBufferLength);
        for (ULONG j = 0; j < inputBufferLength; j++)
        {
            UCHAR byte = *(inputBuffer + j);
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, " %02hhX", byte);
        }
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "\n");

        status = WdfMemoryCopyFromBuffer(
            inputMemory,
            0,
            (PVOID)inputBuffer,
            inputBufferLength
        );

        if (!NT_SUCCESS(status)) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "WdfMemoryCopyFromBuffer failed: 0x%x\n",
                status);

            status = STATUS_UNSUCCESSFUL;
            goto free_input_buffer;
        }

    free_input_buffer:
        ExFreePoolWithTag(inputBuffer, HID_DESCRIPTOR_POOL_TAG);
    }

    if (outputBufferLength > 0)
    {
        outputBuffer = (PUCHAR)ExAllocatePoolWithTag(
            NonPagedPoolNx,
            outputBufferLength,
            HID_DESCRIPTOR_POOL_TAG);

        if (NULL == outputBuffer) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "Could not allocate output buffer!");

            status = STATUS_UNSUCCESSFUL;
            goto exit;
        }

        status = WdfMemoryCopyToBuffer(
            outputMemory,
            Params->Parameters.Ioctl.Output.Offset,
            outputBuffer,
            outputBufferLength);

        if (!NT_SUCCESS(status)) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "WdfMemoryCopyToBuffer failed: 0x%x\n",
                status);

            status = STATUS_UNSUCCESSFUL;
            goto free_output_buffer;
        }

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SOCPARTITION(%d): OUTPUT: LENGTH=%d", Params->Parameters.Ioctl.IoControlCode, outputBufferLength);
        for (ULONG j = 0; j < outputBufferLength; j++)
        {
            UCHAR byte = *(outputBuffer + j);
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, " %02hhX", byte);
        }
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "\n");

        status = WdfMemoryCopyFromBuffer(
            outputMemory,
            0,
            (PVOID)outputBuffer,
            outputBufferLength
        );

        if (!NT_SUCCESS(status)) {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_INIT,
                "WdfMemoryCopyFromBuffer failed: 0x%x\n",
                status);

            status = STATUS_UNSUCCESSFUL;
            goto free_output_buffer;
        }

    free_output_buffer:
        ExFreePoolWithTag(outputBuffer, HID_DESCRIPTOR_POOL_TAG);
    }

exit:
    WdfRequestComplete(Request, status);
    return;
}

VOID
OnContextCleanup(
    IN WDFOBJECT Driver
    )
/*++
Routine Description:

    Free resources allocated in DriverEntry that are not automatically
    cleaned up framework.

Arguments:

    Driver - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    PAGED_CODE();

    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));
}