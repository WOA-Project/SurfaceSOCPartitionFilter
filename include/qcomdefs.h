/*++

Copyright (c) 2021-2024 DuoWoA authors. All Rights Reserved.

Module Name:

	qcomdefs.h

Abstract:

	This file contains the QCOM definitions.

Environment:

	Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <ntdddisk.h>
#include <windef.h>
#include <wdfdriver.h>
#include <ntstrsafe.h>

EXTERN_C_START

// BT.PROVISION

#pragma pack(push)
#pragma pack(1)
typedef struct _BT_PROVISION_DATA
{
	BYTE Version;
	BYTE Length;
	BYTE BDAddress[6];
} BT_PROVISION_DATA, * PBT_PROVISION_DATA;
#pragma pack(pop)

// BT_NVMTAG36.PROVISION

#pragma pack(push)
#pragma pack(1)
typedef struct _BT_NVMTAG36_PROVISION_DATA
{
	BYTE TagNumber;
	BYTE Length;
	BYTE Data[16];
} BT_NVMTAG36_PROVISION_DATA, * PBT_NVMTAG36_PROVISION_DATA;
#pragma pack(pop)

// BT_NVMTAG83.PROVISION

#pragma pack(push)
#pragma pack(1)
typedef struct _BT_NVMTAG83_PROVISION_DATA
{
	BYTE TagNumber;
	BYTE Length;
	BYTE Data[8];
} BT_NVMTAG83_PROVISION_DATA, * PBT_NVMTAG83_PROVISION_DATA;
#pragma pack(pop)

// WLAN.PROVISION

#pragma pack(push)
#pragma pack(1)
typedef struct _WLAN_PROVISION_DATA
{
	BYTE Version;
	BYTE Length;
	BYTE NoOfMACs;
	BYTE StationAddress[6];
	BYTE PeerToPeerDeviceAddress[6]; // Optional, depends on NoOfMacs
	BYTE PeerToPeerClientGoAddress1[6]; // Optional, depends on NoOfMacs
	BYTE PeerToPeerClientGoAddress2[6]; // Optional, depends on NoOfMacs
} WLAN_PROVISION_DATA, * PWLAN_PROVISION_DATA;
#pragma pack(pop)

// WLAN_CLPC.PROVISION Unknown

// WLAN_SAR2CFG.PROVISION

#pragma pack(push)
#pragma pack(1)
typedef struct _WLAN_SAR2CFG_PROVISION_DATA
{
	BYTE HeaderSize;
	BYTE Header1Offset;
	BYTE Header2Offset;
	BYTE WifiTech;
	BYTE ProductId;
	BYTE Version;
	BYTE Revision;
	BYTE NumSarTable;
	BYTE CompressStatus;
	BYTE TimerFormat;
	BYTE Reserved0;
	BYTE Reserved1;
	BYTE Reserved2;
	BYTE Reserved3;
	BYTE Reserved4;
	BYTE Reserved5;
	BYTE UEFIFlagsSize;
	DWORD SARSafetyTimer;
	DWORD SARSafetyRequestResponseTimeout;
	DWORD SARUnsolicitedUpdateTimer;
	BYTE SARState;
	BYTE SleepModeState;
	BYTE SARPowerOnState;
	BYTE SARPowerOnStateAfterFailure;
	BYTE SARSafetyTableIndex;
	BYTE SleepModeStateIndexTable;
	DWORD GeoLocationValue;
	CHAR GeoCountryString[2];
	BYTE SAREnableTestInterface;
} WLAN_SAR2CFG_PROVISION_DATA, * PWLAN_SAR2CFG_PROVISION_DATA;
#pragma pack(pop)

EXTERN_C_END
