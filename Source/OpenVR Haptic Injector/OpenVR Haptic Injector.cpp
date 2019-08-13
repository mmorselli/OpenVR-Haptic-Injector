#include "stdafx.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <wrl\client.h>
#include "WaveBankReader.h"
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"runtimeobject.lib")
#include <Windows.Devices.Enumeration.h>
#include <wrl.h>
#include <ppltasks.h>
#include <sound.h>
#include "SimpleIni.h"
#include <openvr.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Forward declaration
//--------------------------------------------------------------------------------------
HRESULT PlayWaveFromWaveBank(_In_ IXAudio2* pXaudio2, _Inout_ WaveBankReader& wb, _In_ uint32_t index, uint32_t loopcount, float volume);
HRESULT FindMediaFileCch(_Out_writes_(cchDest) WCHAR* strDestPath, _In_ int cchDest, _In_z_ LPCWSTR strFilename);


vr::IVRSystem *vr_sys = nullptr;




int main()
{
	wprintf(L"***************************************************\n");
	wprintf(L"OpenVR Haptic Injector 0.1 Beta\n\n");


	//*********************************************************
	// CONFIG

	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile("config.ini");
	if (rc < 0)
	{
		wprintf(L"Failed to load config.ini");
		return 0;
	}

	const char *  audiodevice = ini.GetValue("main", "audiodevice", " ");
	const char *  soundtest = ini.GetValue("main", "soundtest", "true");

	//wprintf(L"seraching for... %S\n", audiodevice);

	//*********************************************************
	// AUDIO


	ComPtr<IXAudio2> pXAudio2;

	WaveBankReader wb;


	// Initialize XAudio2
	//
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	UINT32 flags = 0;


	hr = XAudio2Create(pXAudio2.GetAddressOf(), flags);
	if (FAILED(hr))
	{
		wprintf(L"Failed to init XAudio2 engine: %#X\n", hr);
		CoUninitialize();
		return 0;
	}


	//
	// Enumerate and display audio devices on the system
	//
	std::vector<AudioDevice> list;
	hr = EnumerateAudio(pXAudio2.Get(), list);
	if (FAILED(hr))
	{
		wprintf(L"Failed to enumerate audio devices: %#X\n", hr);
		CoUninitialize();
		return 0;
	}

	if (hr == S_FALSE)
	{
		wprintf(L"No audio devices found\n");
		CoUninitialize();
		return 0;
	}

	UINT32 devcount = 0;
	UINT32 devindex = -1;

	std::wstring w;
	std::copy(audiodevice, audiodevice + strlen(audiodevice), back_inserter(w));
	const wchar_t *searchstring = w.c_str();

	wprintf(L"***************************************************\n");
	wprintf(L"Scanning audio devices\n");

	for (auto it = list.cbegin(); it != list.cend(); ++it, ++devcount)
	{
		std::wstring deviceid = it->deviceId;
		std::wstring devicedesc = it->description;

		wprintf(L"\nDevice %u\n\tID = \"%s\"\n\tDescription = \"%s\"\n",
			devcount,
			deviceid.c_str(),
			devicedesc.c_str());


		if ((deviceid.find(searchstring) != std::wstring::npos) || (devicedesc.find(searchstring) != std::wstring::npos)) {
			devindex = devcount;
			break;
		}


	}

	wprintf(L"***************************************************\n");
	wprintf(L"\n");

	if (devindex == -1)
	{
		wprintf(L"No device with ID or Description containing: %s\n", searchstring);
		wprintf(L"Use the above data to configure the audiodevice entry in config.ini with a valid string\n");
		wprintf(L"You can use part of ID or Description, the first match wins\n");
		wprintf(L"Don't use the same audio device of your VR headset\n");
		CoUninitialize();
		return 0;
	}
	else {
		wprintf(L"Search string found! Device ID %u for %s\n\n", devindex, searchstring);
	}



	// Create a mastering voice
	//
	IXAudio2MasteringVoice* pMasteringVoice = nullptr;

	if (FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice,
		XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0,
		list[devindex].deviceId.c_str())))
	{
		wprintf(L"Failed creating mastering voice: %#X\n", hr);
		pXAudio2.Reset();
		CoUninitialize();
		return 0;
	}


	// Find our wave bank file
	//
	WCHAR wavebank[MAX_PATH];

	if (FAILED(hr = FindMediaFileCch(wavebank, MAX_PATH, L"waves.xwb")))
	{
		wprintf(L"Failed to find media file (%#X)\n", hr);
		return 0;
	}

	// Extract wavebank data (entries, formats, offsets, and sizes)
	//
	//WaveBankReader wb;

	if (FAILED(hr = wb.Open(wavebank)))
	{
		wprintf(L"Failed to wavebank data (%#X)\n", hr);
		pXAudio2.Reset();
		CoUninitialize();
		return 0;
	}

	// wprintf(L"Wavebank loaded with %u entries.\n", wb.Count());




		wb.WaitOnPrepare();

		// Play sounds from wave bank

		WaveBankReader::Metadata metadata;

		uint32_t j;
		uint32_t loopcount;

		if (strncmp(soundtest, "true",4) == 0)
		{
			wprintf(L"Performing Audio Test\n");

			j = 3;
			loopcount = 2;

			hr = wb.GetMetadata(j, metadata);
			wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
			hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, 0.5f);


			j = 4;
			loopcount = 3;

			hr = wb.GetMetadata(j, metadata);
			wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
			hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, 0.5f);


			j = 2;
			loopcount = 6;

			hr = wb.GetMetadata(j, metadata);
			wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
			hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, 0.5f);
	

			wprintf(L"\n");
		}


	//*********************************************************
	// OPENVR


	// Init OpenVR
	vr::EVRInitError peError;
	vr::VR_Init(&peError, vr::VRApplication_Utility);
	
	
	if (peError != vr::VRInitError_None)
	{
		wprintf(L"can't initialize VR!\n");
		exit(-2);
	}

	wprintf(L"Listening SteamVR Events...\n");
	while (true)
	{

		Sleep(10);
		vr::VREvent_t vrEvent;


		while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vr::VREvent_t)))
		{

			std::string ev = vr::VRSystem()->GetEventTypeNameFromEnum((vr::EVREventType)vrEvent.eventType);
			// wprintf(L"Event Received %hs\n",ev.c_str());

			if (ev == "VREvent_Input_HapticVibration")
			{
				uint64_t componenthandle = vrEvent.data.hapticVibration.componentHandle;
				uint64_t containerhandle = vrEvent.data.hapticVibration.containerHandle;
				float amplitude = vrEvent.data.hapticVibration.fAmplitude;
				float durationseconds = vrEvent.data.hapticVibration.fDurationSeconds;
				float frequency = vrEvent.data.hapticVibration.fFrequency;
				wprintf(L"Haptic Vibration Detected\n");
				wprintf(L"Component: %I64u\n", componenthandle);
				wprintf(L"Container: %I64u\n", containerhandle);
				wprintf(L"Amplitude: %f\n", amplitude);
				wprintf(L"Duration (seconds): %f\n", durationseconds);
				wprintf(L"Frequency: %f\n", frequency);

				j = 2;
				loopcount = 2;

				hr = wb.GetMetadata(j, metadata);
				wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
				hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, 0.5f);


			}
		}
	}

	
}

