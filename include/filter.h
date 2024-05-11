// Copyright (c) Microsoft Corporation. All Rights Reserved.
// Copyright (c) DuoWoA authors. All Rights Reserved.

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <wdmsec.h> // for SDDLs
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <hidport.h>
#include <trace.h>

#define HID_DESCRIPTOR_POOL_TAG 'DdiH'

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DEVICE_CONTEXT_CLEANUP OnContextCleanup;

EVT_WDF_DRIVER_DEVICE_ADD OnDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL OnInternalDeviceControl;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OnIoDeviceControl;

EVT_WDF_REQUEST_COMPLETION_ROUTINE OnRequestCompletionRoutine;