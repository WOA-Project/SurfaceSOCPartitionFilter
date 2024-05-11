/*++

Copyright (c) 2021-2024 DuoWoA authors. All Rights Reserved.

Module Name:

	sfpd.h

Abstract:

	This file contains the SFPD definitions.

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

#define POOL_TAG_FILEPATH  '0PFS'
#define POOL_TAG_DRIVEINFO '1PFS'

#define MAXIMUM_NUMBERS_OF_LUNS 6

#define MAX_FILE_OPEN_ATTEMPTS 10

/*
* Rob Green, a member of the NTDEV list, provides the
* following set of macros that'll keep you from having
* to scratch your head and count zeros ever again.
* Using these defintions, all you'll have to do is write:
*
*interval.QuadPart = RELATIVE(SECONDS(5));
*/
#ifndef ABSOLUTE
#define ABSOLUTE(wait) (wait)
#endif

#ifndef RELATIVE
#define RELATIVE(wait) (-(wait))
#endif

#ifndef NANOSECONDS
#define NANOSECONDS(nanos) \
	(((signed __int64)(nanos)) / 100L)
#endif

#ifndef MICROSECONDS
#define MICROSECONDS(micros) \
	(((signed __int64)(micros)) * NANOSECONDS(1000L))
#endif

#ifndef MILLISECONDS
#define MILLISECONDS(milli) \
	(((signed __int64)(milli)) * MICROSECONDS(1000L))
#endif

#ifndef SECONDS
#define SECONDS(seconds) \
	(((signed __int64)(seconds)) * MILLISECONDS(1000L))
#endif

#define SURFACE_FIRMWARE_PROVISIONING_DATA L"sfpd"

#define ATTESTATION_DATA_DIRECTORY                L"\\attestation" // Epsilon
#define AUDIO_CALIBRATION_FILE_PATH               L"\\audio\\audio.cal" // Zeta
#define BT_NV_FILE_PATH                           L"\\bt\\.bt_nv.bin"
#define LED_CALIBRATION_DATA_FILE_PATH            L"\\camera\\LEDCalibrationData.bin"
#define TOF_FACIAL_CALIBRATION_FILE_PATH          L"\\camera\\tof8801_fac_calib.bin" // Zeta
#define BB_SERIAL_NUMBER_FILE_PATH                L"\\device\\BBSerialNumber.txt" // Zeta
#define DEVICE_COLOR_FILE_PATH                    L"\\device\\DeviceColor.txt"
#define MB_SERIAL_NUMBER_FILE_PATH                L"\\device\\MBSerialNumber.txt" // Zeta
#define PROVISIONING_INFO_FILE_PATH               L"\\device\\ProvisioningInfo.txt"
#define SERIAL_NUMBER_FILE_PATH                   L"\\device\\SerialNumber.txt"
#define CUSTOMER_OS_LUT2_0_FILE_PATH              L"\\display\\CustomerOs_LUT2_0.txt"
#define CUSTOMER_OS_LUT2_1_FILE_PATH              L"\\display\\CustomerOs_LUT2_1.txt"
#define PANEL_CALIBRATION_DATA_C3_FILE_PATH       L"\\display\\disp_user_calib_data_dsi_panel_lg_v2_amoled_cmd_C3.xml" // Epsilon
#define PANEL_CALIBRATION_DATA_R2_FILE_PATH       L"\\display\\disp_user_calib_data_dsi_panel_lg_v2_amoled_cmd_R2.xml" // Epsilon
#define FACTORY_OS_LUT2_0_FILE_PATH               L"\\display\\FactoryOs_LUT2_0.txt"
#define FACTORY_OS_LUT2_1_FILE_PATH               L"\\display\\FactoryOs_LUT2_1.txt"
#define NVRAM_TABLE_0_FILE_PATH                   L"\\display\\NvramTable_0.bin"
#define NVRAM_TABLE_1_FILE_PATH                   L"\\display\\NvramTable_1.bin"
#define PIXEL_ALIGNMENT_DATA_FILE_PATH            L"\\display\\PixelAlignmentData.bin"
#define PANEL_CALIBRATION_DATA_C3_ELGIN_FILE_PATH L"\\display\\qdcm_calib_data_dsi_elgin_dsc_dsi0_c3_cmd.xml" // Zeta
#define PANEL_CALIBRATION_DATA_R2_ELGIN_FILE_PATH L"\\display\\qdcm_calib_data_dsi_elgin_dsc_dsi1_r2_cmd.xml" // Zeta
#define FCC_MODEL_ID_FILE_PATH                    L"\\regulatory\\FccModelId.txt"
#define REGULATORY_LOGOS_FILE_PATH                L"\\regulatory\\RegulatoryLogos.png"
#define SAR_ACTION_TABLE_FILE_PATH                L"\\sar\\SarMgrActionTable.bin"
#define SAR_LTE_REGION_FILE_PATH                  L"\\sar\\SarMgrLTERegion.bin"
#define SAR_TRIGGER_TABLE_FILE_PATH               L"\\sar\\SarMgrTriggerTable.bin"
#define SAR_WIFI_REGION_FILE_PATH                 L"\\sar\\SarMgrWifiRegion.bin"
#define WCNSS_CONFIG_FILE_PATH                    L"\\sar\\WCNSS_qcom_cfg.ini"
#define WIFI_REGION_CONFIG_FILE_PATH              L"\\sar\\WifiRegionConfig.bin"
#define WIFI_SAR_CONFIG_FILE_PATH                 L"\\sar\\WifiSARConfig.bin"
#define WIFI_SAR_HEADER_FILE_PATH                 L"\\sar\\WifiSARHeader.bin"
#define WIFI_SAR_TABLE_FILE_PATH                  L"\\sar\\WifiSARTable.bin"
#define SENSOR_DATA_DIRECTORY                     L"\\sensors"
#define WIDEVINE_DATA_DIRECTORY                   L"\\widevine" // Epsilon
#define WLAN_MAC_FILE_PATH                        L"\\wlan\\wlan_mac.bin"

typedef enum _SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA_PANEL_SIDE
{
	UNKNOWN_SIDE,
	LEFT_SIDE = 1,
	RIGHT_SIDE = 2,
	MAX_DISPLAY_SIDES = RIGHT_SIDE
} SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA_PANEL_SIDE;

#pragma pack(push)
#pragma pack(1)
typedef struct _SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA
{
	BYTE Reserved0[4]; // 0
	BYTE Reserved1[4]; // 31 37 17 C2
	SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA_PANEL_SIDE Panel0Side;
	SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA_PANEL_SIDE Panel1Side;
	BYTE Panel0Shift[4];
	BYTE Panel1Shift[4];
} SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA, * PSFPD_DISPLAY_PIXEL_ALIGNMENT_DATA;
#pragma pack(pop)

NTSTATUS GetSFPDPixelAlignmentData(WDFDEVICE device, PSFPD_DISPLAY_PIXEL_ALIGNMENT_DATA PixelAlignmentData);
NTSTATUS GetSFPDVolumePath(WDFDEVICE device, WCHAR* VolumePath, DWORD VolumePathLength);
NTSTATUS GetSFPDItemSize(WDFDEVICE device, WCHAR* ItemPath, DWORD* ItemSize);
NTSTATUS GetSFPDItem(WDFDEVICE device, WCHAR* ItemPath, PVOID Data, DWORD DataSize);

EXTERN_C_END
