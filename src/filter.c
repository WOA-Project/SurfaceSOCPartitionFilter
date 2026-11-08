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
#include <qcomdefs.h>
#include <sfpd.h>

#include <constants.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, OnDeviceAdd)
#pragma alloc_text (PAGE, OnInternalDeviceControl)
#pragma alloc_text (PAGE, OnContextCleanup)
#endif

typedef struct _REQUEST_CONTEXT
{
    ULONG InputBufferLength;
} REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    REQUEST_CONTEXT,
    GetRequestContext);

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

	WDF_OBJECT_ATTRIBUTES attributes;

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
		&attributes,
		REQUEST_CONTEXT);

	WdfDeviceInitSetRequestAttributes(
		DeviceInit,
		&attributes);

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
		"OnIoDeviceControl: OutputBufferLength: %Iu, InputBufferLength: %Iu, IoControlCode: 0x%08X\n",
		OutputBufferLength, InputBufferLength, IoControlCode);

	//
	// Please note that QCSOCPartition provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl. So depending on the ioctl code, we will either
	// use retreive function or escape to WDM to get the UserBuffer.
	//

	switch (IoControlCode) {
	case 0xECAF32C2:
	case 0xECAF32C6:
	case 0xECAF32CE:
		//
		// Obtains the report descriptor for the HID device
		//
		forwardWithCompletionRoutine = TRUE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"OnIoDeviceControl: forwardWithCompletionRoutine = TRUE!\n");
	    break;
	
	default:
	    break;
	}

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

		REQUEST_CONTEXT* context = GetRequestContext(Request);
		context->InputBufferLength = InputBufferLength;

		//
		// Set our completion routine with a context area that we will save
		// the output data into
		//
		WdfRequestSetCompletionRoutine(
			Request,
			OnRequestCompletionRoutine,
			context);

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
OnRequestCompletionRoutine(
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
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    NTSTATUS filterStatus;

    PUCHAR inputBuffer = NULL;
    PUCHAR outputBuffer = NULL;

	if (Params == NULL)
	{
		goto cleanup;
	}

	if (Context == NULL)
	{
		goto cleanup;
	}

	WDFDEVICE device = WdfIoTargetGetDevice(Target);
	if (device == NULL)
	{
		goto cleanup;
	}

	// The result of the IOCTL call to the SOCPartition driver
	status = Params->IoStatus.Status;

	// The input buffer for the IOCTL call to the SOCPartition driver
	WDFMEMORY inputMemory = Params->Parameters.Ioctl.Input.Buffer;

	// The output buffer for the IOCTL call to the SOCPartition driver
	WDFMEMORY outputMemory = Params->Parameters.Ioctl.Output.Buffer;

	// The length of the input buffer sent to SOCPartition
	ULONG inputBufferLength = ((REQUEST_CONTEXT*)Context)->InputBufferLength;

	// The length of the output buffer sent to SOCPartition
	ULONG outputBufferLength = (ULONG)Params->Parameters.Ioctl.Output.Length;

	ULONG IoControlCode = Params->Parameters.Ioctl.IoControlCode;

	// Check the for the buffer lengths, which must be at least 296 respectively (header structure)
	if (inputBufferLength < 296 || outputBufferLength < 296)
	{
		goto cleanup;
	}

	if (IoControlCode != 0xECAF32C2 && // ReadFile
		IoControlCode != 0xECAF32CE && // ListDirectoryFiles
		IoControlCode != 0xECAF32C6) // GetFileProperty
	{
		// We only handle above IOCTLs. If it doesn't match, return now.
		goto cleanup;
	}

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_INIT,
		"OnRequestCompletionRoutine: IoControlCode: %d - Status: %d\n",
		IoControlCode,
		Params->IoStatus.Status);

	// Retrieve the input buffer, as an array of BYTE
	inputBuffer = (PUCHAR)ExAllocatePoolWithTag(
		NonPagedPoolNx,
		inputBufferLength,
		HID_DESCRIPTOR_POOL_TAG);

	if (NULL == inputBuffer) {
    	status = STATUS_INSUFFICIENT_RESOURCES;

		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not allocate input buffer!");

    	goto cleanup;
	}

	filterStatus = WdfMemoryCopyToBuffer(
		inputMemory,
		Params->Parameters.Ioctl.Input.Offset,
		inputBuffer,
		inputBufferLength);

	if (!NT_SUCCESS(filterStatus)) {
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"WdfMemoryCopyToBuffer failed: 0x%x\n",
			filterStatus);

		status = filterStatus;
		goto cleanup;
	}

	// Retrieve the output buffer, as an array of BYTE
	outputBuffer = (PUCHAR)ExAllocatePoolWithTag(
		NonPagedPoolNx,
		outputBufferLength,
		HID_DESCRIPTOR_POOL_TAG);

	if (NULL == outputBuffer) {
    	status = STATUS_INSUFFICIENT_RESOURCES;

		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not allocate output buffer!");

		goto cleanup;
	}

	filterStatus = WdfMemoryCopyToBuffer(
		outputMemory,
		Params->Parameters.Ioctl.Output.Offset,
		outputBuffer,
		outputBufferLength);

	if (!NT_SUCCESS(filterStatus)) {
    	status = filterStatus;

		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"WdfMemoryCopyToBuffer failed: 0x%x\n",
			filterStatus);

		goto cleanup;
	}

	// Check the output buffer provided IOCTL, it must match the input.
	ULONG OutputBufferIOCTL = *(ULONG*)(outputBuffer);

	if (OutputBufferIOCTL != IoControlCode)
	{
		goto cleanup;
	}

	// Now check the output buffer provided return code.
	// We will only deal with non successful codes in our filter
	NTSTATUS OutputBufferStatus = *(NTSTATUS*)(outputBuffer + 4);

	if (NT_SUCCESS(OutputBufferStatus))
	{
		goto cleanup;
	}

	// We know that we have a non successful valid request to SOCPartition at the moment.
	// Handle it on our own :)

	switch (IoControlCode)
	{
	case 0xECAF32C2: // ReadFile
	{
		PWCHAR FilePath = (PWCHAR)(inputBuffer + 88); // 96 size

		if (RtlCompareMemory(L"QCOM\\BT_NVMTAG36.PROVISION", FilePath, sizeof(L"QCOM\\BT_NVMTAG36.PROVISION")) == sizeof(L"QCOM\\BT_NVMTAG36.PROVISION"))
		{
			// Buffer too small
			if (outputBufferLength < 296 + sizeof(BT_NVMTAG36_PROVISION))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = sizeof(BT_NVMTAG36_PROVISION);
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(BT_NVMTAG36_PROVISION);

				// File Data
				RtlCopyMemory(outputBuffer + 20, BT_NVMTAG36_PROVISION, sizeof(BT_NVMTAG36_PROVISION));
			}
		}
		else if (RtlCompareMemory(L"QCOM\\BT_NVMTAG83.PROVISION", FilePath, sizeof(L"QCOM\\BT_NVMTAG83.PROVISION")) == sizeof(L"QCOM\\BT_NVMTAG83.PROVISION"))
		{
			// Buffer too small
			if (outputBufferLength < 296 + sizeof(BT_NVMTAG83_PROVISION))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = sizeof(BT_NVMTAG83_PROVISION);
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(BT_NVMTAG83_PROVISION);

				// File Data
				RtlCopyMemory(outputBuffer + 20, BT_NVMTAG83_PROVISION, sizeof(BT_NVMTAG83_PROVISION));
			}
		}
		else if (RtlCompareMemory(L"QCOM\\BT.PROVISION", FilePath, sizeof(L"QCOM\\BT.PROVISION")) == sizeof(L"QCOM\\BT.PROVISION"))
		{
			// Buffer too small
			if (outputBufferLength < 296 + sizeof(BT_PROVISION))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = sizeof(BT_PROVISION);
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(BT_PROVISION);

				// Fill in the real BT MAC
				BYTE BT_NV[9] = { 0 };

				BYTE LocalBtProvision[sizeof(BT_PROVISION)];

				RtlCopyMemory(
					LocalBtProvision,
					BT_PROVISION,
					sizeof(BT_PROVISION));

				filterStatus = GetSFPDItem(device, BT_NV_FILE_PATH, BT_NV, sizeof(BT_NV));
				if (NT_SUCCESS(filterStatus))
				{
					LocalBtProvision[2] = BT_NV[8];
					LocalBtProvision[3] = BT_NV[7];
					LocalBtProvision[4] = BT_NV[6];
					LocalBtProvision[5] = BT_NV[5];
					LocalBtProvision[6] = BT_NV[4];
					LocalBtProvision[7] = BT_NV[3];
				}
				else
				{
					status = filterStatus;
					goto cleanup;
				}

				// File Data
				RtlCopyMemory(outputBuffer + 20, LocalBtProvision, sizeof(LocalBtProvision));
			}
		}
		// Sensor files
		else if (RtlCompareMemory(L"JSON\\", FilePath, sizeof(L"JSON\\")) == (sizeof(L"JSON\\") - sizeof(WCHAR)))
		{
			WCHAR SensorFilePath[92 + (sizeof(SENSOR_DATA_DIRECTORY) - sizeof(WCHAR)) / sizeof(WCHAR)] = SENSOR_DATA_DIRECTORY;
			RtlCopyMemory(SensorFilePath + (sizeof(SENSOR_DATA_DIRECTORY) - sizeof(WCHAR)) / sizeof(WCHAR), FilePath + 4, 92);

			SensorFilePath[
				RTL_NUMBER_OF(SensorFilePath) - 1
			] = UNICODE_NULL;

			ULONG ItemSize = 0;

			if (device == NULL)
			{
				goto cleanup;
			}

			filterStatus = GetSFPDItemSize(device, SensorFilePath, &ItemSize);

			if (!NT_SUCCESS(filterStatus))
			{
				status = filterStatus;
				goto cleanup;
			}

			// Size is not enough
			if (outputBufferLength - 20 < ItemSize)
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = ItemSize;
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = ItemSize;

				filterStatus = GetSFPDItem(device, SensorFilePath, outputBuffer + 20, ItemSize);

				if (!NT_SUCCESS(filterStatus))
				{
					status = filterStatus;
					goto cleanup;
				}
			}
		}
		else if (RtlCompareMemory(L"QCOM\\WLAN_PMICXO.PROVISION", FilePath, sizeof(L"QCOM\\WLAN_PMICXO.PROVISION")) == sizeof(L"QCOM\\WLAN_PMICXO.PROVISION"))
		{
			status = STATUS_FILE_NOT_AVAILABLE;

			RtlZeroMemory(outputBuffer, outputBufferLength);

			// IOCTL
			*(ULONG*)(outputBuffer) = IoControlCode;

			// Status
			*(NTSTATUS*)(outputBuffer + 4) = STATUS_FILE_NOT_AVAILABLE;

			// Needed Buffer Size
			*(ULONG*)(outputBuffer + 8) = 0;

			if (outputBufferLength > 296)
			{
				// Data Size
				*(ULONG*)(outputBuffer + 16) = 1;

				// File Data
				*(ULONG*)(outputBuffer + 20) = 0;
			}
		}
		else if (RtlCompareMemory(L"QCOM\\WLAN.PROVISION", FilePath, sizeof(L"QCOM\\WLAN.PROVISION")) == sizeof(L"QCOM\\WLAN.PROVISION"))
		{
			// Buffer too small
			if (outputBufferLength < 296 + sizeof(WLAN_PROVISION))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = sizeof(WLAN_PROVISION);
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(WLAN_PROVISION);

				// Fill in the real BT MAC
				BYTE WLAN_MAC[33] = { 0 };

				BYTE LocalWlanProvision[sizeof(WLAN_PROVISION)];

				RtlCopyMemory(
					LocalWlanProvision,
					WLAN_PROVISION,
					sizeof(WLAN_PROVISION));

				filterStatus = GetSFPDItem(device, WLAN_MAC_FILE_PATH, WLAN_MAC, sizeof(WLAN_MAC));
				if (NT_SUCCESS(filterStatus))
				{
					BYTE MAC_ADDRESS[6] = { 0 };

					for (ULONG i = 0; i < sizeof(MAC_ADDRESS); i++)
					{
						ULONG HighIndex = 16 + i * 2;
						ULONG LowIndex = 16 + (i * 2) + 1;

						ULONG ByteHigh = WLAN_MAC[HighIndex];
						ULONG ByteLow = WLAN_MAC[LowIndex];

						if (0x30 <= ByteHigh && ByteHigh <= 0x39)
						{
							MAC_ADDRESS[i] |= ((ByteHigh - 0x30) << 4) & 0xF0;
						}
						else if (0x41 <= ByteHigh && ByteHigh <= 0x46)
						{
							MAC_ADDRESS[i] |= ((ByteHigh - 0x37) << 4) & 0xF0;
						}
						else
						{
							goto skip_mac;
						}

						if (0x30 <= ByteLow && ByteLow <= 0x39)
						{
							MAC_ADDRESS[i] |= (ByteLow - 0x30) & 0x0F;
						}
						else if (0x41 <= ByteLow && ByteLow <= 0x46)
						{
							MAC_ADDRESS[i] |= (ByteLow - 0x37) & 0x0F;
						}
						else
						{
							goto skip_mac;
						}
					}

					LocalWlanProvision[3] = MAC_ADDRESS[0];
					LocalWlanProvision[4] = MAC_ADDRESS[1];
					LocalWlanProvision[5] = MAC_ADDRESS[2];
					LocalWlanProvision[6] = MAC_ADDRESS[3];
					LocalWlanProvision[7] = MAC_ADDRESS[4];
					LocalWlanProvision[8] = MAC_ADDRESS[5];
				}
				else
				{
					status = filterStatus;
					goto cleanup;
				}

skip_mac:
				// File Data
				RtlCopyMemory(outputBuffer + 20, LocalWlanProvision, sizeof(LocalWlanProvision));
			}
		}
		else if (RtlCompareMemory(L"QCOM\\WLAN_CLPC.PROVISION", FilePath, sizeof(L"QCOM\\WLAN_CLPC.PROVISION")) == sizeof(L"QCOM\\WLAN_CLPC.PROVISION"))
		{
			// Buffer too small
			if (outputBufferLength < 296 + sizeof(WLAN_CLPC_PROVISION))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = sizeof(WLAN_CLPC_PROVISION);
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(WLAN_CLPC_PROVISION);

				// File Data
				RtlCopyMemory(outputBuffer + 20, WLAN_CLPC_PROVISION, sizeof(WLAN_CLPC_PROVISION));
			}
		}
		else if (RtlCompareMemory(L"QCOM\\WLAN_SAR2CFG.PROVISION", FilePath, sizeof(L"QCOM\\WLAN_SAR2CFG.PROVISION")) == sizeof(L"QCOM\\WLAN_SAR2CFG.PROVISION"))
		{
			// Buffer too small
			if (outputBufferLength < 296 + sizeof(WLAN_SAR2CFG_PROVISION))
			{
				status = STATUS_SUCCESS;

				// IOCTL
				RtlZeroMemory(outputBuffer, outputBufferLength);

				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = sizeof(WLAN_SAR2CFG_PROVISION);
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(WLAN_SAR2CFG_PROVISION);

				// File Data
				RtlCopyMemory(outputBuffer + 20, WLAN_SAR2CFG_PROVISION, sizeof(WLAN_SAR2CFG_PROVISION));
			}
		}
		else
		{
			// We do not support anything else currently.
			goto cleanup;
		}

		break;
	}
	case 0xECAF32CE: // ListDirectoryFiles
	{
		PWCHAR FilePath = (PWCHAR)(inputBuffer + 88); // 96 size
		ULONG FileSystemProperty = *(PULONG)(inputBuffer + 284);

		if (FileSystemProperty != 10)
		{
			// We only support number of files currently
			goto cleanup;
		}

		if (RtlCompareMemory(L"JSON", FilePath, sizeof(L"JSON")) == sizeof(L"JSON"))
		{
			ULONG NumberOfFiles = 0;

			if (device == NULL)
			{
				goto cleanup;
			}

			filterStatus = GetSFPDNumberOfFilesInDirectory(device, SENSOR_DATA_DIRECTORY, &NumberOfFiles);

			if (!NT_SUCCESS(filterStatus))
			{
				status = filterStatus;
				goto cleanup;
			}

			ULONG TotalNeededBufferSize;
			status = RtlULongMult(
				NumberOfFiles,
				244,
				&TotalNeededBufferSize);

			if (!NT_SUCCESS(status))
			{
				goto cleanup;
			}

			// Buffer too small
			if (outputBufferLength < 296 + TotalNeededBufferSize)
			{
				status = STATUS_BUFFER_TOO_SMALL;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = TotalNeededBufferSize;
			}
			else
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = TotalNeededBufferSize;

				// Fill it in!
				filterStatus = GetSFPDFilesInDirectory(device, SENSOR_DATA_DIRECTORY, NumberOfFiles, outputBuffer + 20);

				if (!NT_SUCCESS(filterStatus))
				{
					status = filterStatus;
					goto cleanup;
				}
			}
		}
		else
		{
			// We do not support anything else currently.
			goto cleanup;
		}

		break;
	}
	case 0xECAF32C6: // GetFileProperty
	{
		PWCHAR FilePath = (PWCHAR)(inputBuffer + 88); // 96 size
		ULONG FileProperty = *(PULONG)(inputBuffer + 280);

		if (FileProperty != 2)
		{
			// We only support actual file size currently
			goto cleanup;
		}

		// Buffer too small
		if (outputBufferLength < 296 + sizeof(ULONG))
		{
			status = STATUS_SUCCESS;

			RtlZeroMemory(outputBuffer, outputBufferLength);

			// IOCTL
			*(ULONG*)(outputBuffer) = IoControlCode;

			// Status
			*(NTSTATUS*)(outputBuffer + 4) = STATUS_BUFFER_TOO_SMALL;

			// Needed Buffer Size
			*(ULONG*)(outputBuffer + 8) = sizeof(ULONG);
		}
		else
		{
			if (RtlCompareMemory(L"QCOM\\BT_NVMTAG36.PROVISION", FilePath, sizeof(L"QCOM\\BT_NVMTAG36.PROVISION")) == sizeof(L"QCOM\\BT_NVMTAG36.PROVISION"))
			{
				status = STATUS_SUCCESS;

				// IOCTL
				RtlZeroMemory(outputBuffer, outputBufferLength);

				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = sizeof(BT_NVMTAG36_PROVISION);
			}
			else if (RtlCompareMemory(L"QCOM\\BT_NVMTAG83.PROVISION", FilePath, sizeof(L"QCOM\\BT_NVMTAG83.PROVISION")) == sizeof(L"QCOM\\BT_NVMTAG83.PROVISION"))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = sizeof(BT_NVMTAG83_PROVISION);
			}
			else if (RtlCompareMemory(L"QCOM\\BT.PROVISION", FilePath, sizeof(L"QCOM\\BT.PROVISION")) == sizeof(L"QCOM\\BT.PROVISION"))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = sizeof(BT_PROVISION);
			}
			// Sensor files
			else if (RtlCompareMemory(L"JSON\\", FilePath, sizeof(L"JSON\\")) == sizeof(L"JSON\\"))
			{
				WCHAR SensorFilePath[92 + (sizeof(SENSOR_DATA_DIRECTORY) - sizeof(WCHAR)) / sizeof(WCHAR)] = SENSOR_DATA_DIRECTORY;
				RtlCopyMemory(SensorFilePath + (sizeof(SENSOR_DATA_DIRECTORY) - sizeof(WCHAR)) / sizeof(WCHAR), FilePath + 4, 92);

				SensorFilePath[
					RTL_NUMBER_OF(SensorFilePath) - 1
				] = UNICODE_NULL;

				ULONG ItemSize = 0;

				if (device == NULL)
				{
					goto cleanup;
				}

				filterStatus = GetSFPDItemSize(device, SensorFilePath, &ItemSize);

				if (!NT_SUCCESS(filterStatus))
				{
					status = filterStatus;
					goto cleanup;
				}

				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = ItemSize;
			}
			else if (RtlCompareMemory(L"QCOM\\WLAN_PMICXO.PROVISION", FilePath, sizeof(L"QCOM\\WLAN_PMICXO.PROVISION")) == sizeof(L"QCOM\\WLAN_PMICXO.PROVISION"))
			{
				status = STATUS_FILE_NOT_AVAILABLE;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_FILE_NOT_AVAILABLE;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = 1;

				// File Size
				*(ULONG*)(outputBuffer + 20) = 0;
			}
			else if (RtlCompareMemory(L"QCOM\\WLAN.PROVISION", FilePath, sizeof(L"QCOM\\WLAN.PROVISION")) == sizeof(L"QCOM\\WLAN.PROVISION"))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = sizeof(WLAN_PROVISION);
			}
			else if (RtlCompareMemory(L"QCOM\\WLAN_CLPC.PROVISION", FilePath, sizeof(L"QCOM\\WLAN_CLPC.PROVISION")) == sizeof(L"QCOM\\WLAN_CLPC.PROVISION"))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = sizeof(WLAN_CLPC_PROVISION);
			}
			else if (RtlCompareMemory(L"QCOM\\WLAN_SAR2CFG.PROVISION", FilePath, sizeof(L"QCOM\\WLAN_SAR2CFG.PROVISION")) == sizeof(L"QCOM\\WLAN_SAR2CFG.PROVISION"))
			{
				status = STATUS_SUCCESS;

				RtlZeroMemory(outputBuffer, outputBufferLength);

				// IOCTL
				*(ULONG*)(outputBuffer) = IoControlCode;

				// Status
				*(NTSTATUS*)(outputBuffer + 4) = STATUS_SUCCESS;

				// Needed Buffer Size
				*(ULONG*)(outputBuffer + 8) = 0;

				// Data Size
				*(ULONG*)(outputBuffer + 16) = sizeof(ULONG);

				// File Size
				*(ULONG*)(outputBuffer + 20) = sizeof(WLAN_SAR2CFG_PROVISION);
			}
			else
			{
				// We do not support anything else currently.
				goto cleanup;
			}
		}

		break;
	}
	}

	// Write out output buffer back to the output buffer for completion
	// We are meant to modify it.
	filterStatus = WdfMemoryCopyFromBuffer(
		outputMemory,
		0,
		(PVOID)outputBuffer,
		outputBufferLength
	);

	if (!NT_SUCCESS(filterStatus)) {
		status = filterStatus;

		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"WdfMemoryCopyFromBuffer failed: 0x%x\n",
			filterStatus);

		goto cleanup;
	}

cleanup:

    if (outputBuffer != NULL)
    {
        ExFreePoolWithTag(
            outputBuffer,
            HID_DESCRIPTOR_POOL_TAG);
    }

    if (inputBuffer != NULL)
    {
        ExFreePoolWithTag(
            inputBuffer,
            HID_DESCRIPTOR_POOL_TAG);
    }

    WdfRequestComplete(
        Request,
        status);

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