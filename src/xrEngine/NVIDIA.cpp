//////////////////////////////////////////
// Desc:   GPU Info
// Model:  NVIDIA
// Author: ForserX, Mortan
//////////////////////////////////////////
// Oxygen Engine (2016-2019)
//////////////////////////////////////////
#include "stdafx.h"
#include "NVIDIA.h"

CNvReader* NvData = nullptr;
bool CNvReader::bSupport = false;
static HINSTANCE hDLL;

CNvReader::CNvReader() : AdapterID(0), AdapterFinal(0), gpuHandlesPh{}, gpuHandlesLg{}, gpuUsages{}, NvAPI_GPU_PhysicalFromLogical(nullptr)
{
	hDLL = LoadLibraryA("nvapi64.dll");
	if (!bSupport && hDLL)
	{
		bSupport = true;

		NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(hDLL, "nvapi_QueryInterface");
		NvAPI_Initialize = (NvAPI_Initialize_t)(*NvAPI_QueryInterface)(0x0150E828);
		NvAPI_EnumPhysicalGPUs = (NvAPI_EnumPhysicalGPUs_t)(*NvAPI_QueryInterface)(0xE5AC921F);
		NvAPI_EnumLogicalGPUs = (NvAPI_EnumLogicalGPUs_t)(*NvAPI_QueryInterface)(0x48B3EA59);
		NvAPI_GPU_GetUsages = (NvAPI_GPU_GetUsages_t)(*NvAPI_QueryInterface)(0x189A1FDF);
		NvAPI_GPU_PhysicalFromLogical = (NvAPI_PhysicalFromLogical)(*NvAPI_QueryInterface)(0x0AEA3FA32);
		//OldSerpskiStalker
		NvAPI_GPU_GetThermalSettings = (NvAPI_GPU_GetThermalSettings_t)(*NvAPI_QueryInterface)(0xE3640A56);

		InitDeviceInfo();
	}
}

CNvReader::~CNvReader()
{
	if (hDLL)
		FreeLibrary(hDLL);
}

void CNvReader::InitDeviceInfo()
{
	(*NvAPI_Initialize)();

	// gpuUsages[0] must be this value, otherwise NvAPI_GPU_GetUsages won't work
	gpuUsages[0] = (NVAPI_MAX_USAGES_PER_GPU * 4) | 0x10000;

	(*NvAPI_EnumPhysicalGPUs)(gpuHandlesPh, &AdapterID);

	if (NvAPI_GPU_PhysicalFromLogical)
		MakeGPUCount();
}

void CNvReader::MakeGPUCount()
{
	NvU32 logicalGPUCount;
	NvAPI_EnumLogicalGPUs(gpuHandlesLg, &logicalGPUCount);

	for (NvU32 i = 0; i < logicalGPUCount; ++i)
	{
		NvAPI_GPU_PhysicalFromLogical(gpuHandlesLg[i], gpuHandlesPh, &AdapterID);
		AdapterFinal = std::max(AdapterFinal, (u64)AdapterID);
	}

	if (AdapterFinal > 1)
	{
		Msg("# [MSG] NVidia MGPU: %d-Way SLI detected.", AdapterFinal);
	}
}

u32 CNvReader::GetPercentActive()
{
	(*NvAPI_GPU_GetUsages)(gpuHandlesPh[0], gpuUsages);
	int usage = gpuUsages[3];
	return (u32)usage;
}

u32 CNvReader::GetGPUCount()
{
	return u32(AdapterFinal ? AdapterFinal : 1);
}

//-' OldSerpskiStalker
//-' https://stackoverflow.com/questions/25376080/nvcplgetthermalsettings-call-to-nvcpl-dll-returns-false-c
u32 CNvReader::GetTemperature()
{
	// �������� ������ � ���������
	NvU32 logicalGPUCount;
	NV_GPU_THERMAL_SETTINGS nvgts;

	// ������ ���������� ���������
	(*NvAPI_EnumPhysicalGPUs)(gpuHandlesPh, &logicalGPUCount);
	nvgts.version = sizeof(NV_GPU_THERMAL_SETTINGS) | (1 << 16);
	nvgts.count = 0;
	nvgts.sensor[0].controller = NVAPI_THERMAL_CONTROLLER_GPU_INTERNAL;
	nvgts.sensor[0].target = NVAPI_THERMAL_TARGET_GPU;

	// ���������� ��� � 100 �����������
	for (size_t i = 0; i < 100; i++)
	{
		(*NvAPI_GPU_GetThermalSettings)(gpuHandlesPh[0], 0, &nvgts);
		return (u32)(nvgts.sensor[0].currentTemp);
	}

	// �� ����� �� �������� �� ������� �������� ���������, ������ u32
	return u32(-1);
}

