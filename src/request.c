//
// request.c
//

#include "filter.h"

#include "request.h"

#include "request_context.h"

#include <request.tmh>

NTSTATUS
FilterContextInitialize(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT RequestContext,
    _Out_ PFILTER_CONTEXT Context)
{
    NTSTATUS status;
    REQUEST_CONTEXT *RequestCtx;

    if (Context == NULL || Params == NULL || RequestContext == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Context, sizeof(*Context));

    RequestCtx = (REQUEST_CONTEXT *)RequestContext;

    Context->Request = Request;
    Context->Device = WdfIoTargetGetDevice(Target);
    Context->InputMemory = Params->Parameters.Ioctl.Input.Buffer;
    Context->OutputMemory = Params->Parameters.Ioctl.Output.Buffer;
    Context->InputLength = RequestCtx->InputBufferLength;
    Context->OutputLength = (ULONG)Params->Parameters.Ioctl.Output.Length;
    Context->Ioctl = Params->Parameters.Ioctl.IoControlCode;

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_DRIVER,
        "ContextInitialize Ioctl=0x%08X In=%lu Out=%lu",
        Context->Ioctl,
        Context->InputLength,
        Context->OutputLength);

    if (Context->Device == NULL)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (Context->InputMemory == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Context->OutputMemory == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Context->InputLength == 0)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    if (Context->OutputLength == 0)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    Context->InputBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, Context->InputLength, HID_DESCRIPTOR_POOL_TAG);
    if (Context->InputBuffer == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Context->OutputBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, Context->OutputLength, HID_DESCRIPTOR_POOL_TAG);
    if (Context->OutputBuffer == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto failure;
    }

    RtlZeroMemory(Context->OutputBuffer, Context->OutputLength);

    status = WdfMemoryCopyToBuffer(
        Context->InputMemory, Params->Parameters.Ioctl.Input.Offset, Context->InputBuffer, Context->InputLength);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Input copy failed 0x%08X", status);
        goto failure;
    }

    status = WdfMemoryCopyToBuffer(
        Context->OutputMemory, Params->Parameters.Ioctl.Output.Offset, Context->OutputBuffer, Context->OutputLength);
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Output copy failed 0x%08X", status);
        goto failure;
    }

    return STATUS_SUCCESS;

failure:

    FilterContextDestroy(Context);

    return status;
}

VOID
FilterContextDestroy(_In_ PFILTER_CONTEXT Context)
{
    if (Context == NULL)
    {
        return;
    }

    Trace(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "DestroyContext");

    if (Context->OutputBuffer != NULL)
    {
        ExFreePoolWithTag(Context->OutputBuffer, HID_DESCRIPTOR_POOL_TAG);
        Context->OutputBuffer = NULL;
    }

    if (Context->InputBuffer != NULL)
    {
        ExFreePoolWithTag(Context->InputBuffer, HID_DESCRIPTOR_POOL_TAG);
        Context->InputBuffer = NULL;
    }

    Context->InputLength = 0;
    Context->OutputLength = 0;
}

NTSTATUS
GetInputUlong(_In_ PFILTER_CONTEXT Context, _In_ ULONG Offset, _Out_ PULONG Value)
{
    if (Context == NULL || Value == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if ((Offset + sizeof(ULONG)) > Context->InputLength)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    *Value = *(UNALIGNED ULONG *)(Context->InputBuffer + Offset);

    return STATUS_SUCCESS;
}

NTSTATUS
GetInputPath(_In_ PFILTER_CONTEXT Context, _Out_ PPARSED_PATH ParsedPath)
{
    if (Context == NULL || ParsedPath == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    return ExtractPath(Context->InputBuffer, Context->InputLength, ParsedPath);
}