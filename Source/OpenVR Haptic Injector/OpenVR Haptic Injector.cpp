#include "stdafx.h"
#include <map>
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




int main(int argc, char *argv[])
{
	wprintf(L"***************************************************\n");
	wprintf(L"OpenVR Haptic Injector 0.2 Beta\n\n");




	

	//*********************************************************
	// CONFIG

	CSimpleIniA ini;
	std::string keyvalue;
	const char * inifile;

	if (argc > 1)
	{
		inifile = argv[1];
	}
	else {
		inifile = "config.ini";
	}


	SI_Error rc = ini.LoadFile(inifile);
	if (rc < 0)
	{
		std::cout << "Failed to load " << inifile << "\n";
		return 0;
	}

	keyvalue = ini.GetValue("main", "profile", " ");
	std::cout << "Profile loaded: " << keyvalue << "\n";

	const char *  audiodevice = ini.GetValue("main", "audiodevice", " "); // search string for audio device ID or Description
	const char *  soundtest = ini.GetValue("main", "soundtest", "true"); // perform sound test at startup

	// haptic mapping
	std::map <std::string, std::string> maps;

	// get all keys in maps section
	CSimpleIniA::TNamesDepend keys;
	ini.GetAllKeys("maps", keys);



	// load all keys
	CSimpleIniA::TNamesDepend::const_iterator i;
	for (i = keys.begin(); i != keys.end(); ++i) {
		// printf("key-name = '%s' - ", i->pItem);
		keyvalue = ini.GetValue("maps", i->pItem, " ");
		// printf("key-value = '%s'\n", keyvalue);
		maps.insert(std::pair<std::string, std::string>(i->pItem, keyvalue));
	}


	//*********************************************************
	// TEST ZONE

	/*

	*/

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


	keyvalue = ini.GetValue("default", "xwb", "waves.xwb");

	LPCWSTR xwbfile = std::wstring(keyvalue.begin(), keyvalue.end()).c_str();

	WCHAR wavebank[MAX_PATH];

	if (FAILED(hr = FindMediaFileCch(wavebank, MAX_PATH, xwbfile)))
	{
		std::cout << "Failed to find Wave Banks " << keyvalue << "\n";
		return 0;
	}
	else {
		std::cout << "Wave Banks " << keyvalue << " loaded" << "\n";
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
		float volume;

		// default values
		uint32_t defj;
		uint32_t defloopcount;
		float defvolume;

		keyvalue = ini.GetValue("default", "index", "0");
		defj = std::stoi(keyvalue);
		keyvalue = ini.GetValue("default", "loopcount", "1");
		defloopcount = std::stoi(keyvalue);
		keyvalue = ini.GetValue("default", "volume", "1");
		defvolume = std::stof(keyvalue);


		if (strncmp(soundtest, "true",4) == 0)
		{
			wprintf(L"Performing Audio Test\n");

			j = 0;
			loopcount = 1;
			volume = 0.2f;

			hr = wb.GetMetadata(j, metadata);
			wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
			hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, volume);


			j = 1;
			loopcount = 3;

			hr = wb.GetMetadata(j, metadata);
			wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
			hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, volume);


			j = 2;
			loopcount = 6;

			hr = wb.GetMetadata(j, metadata);
			wprintf(L"Play entry %u from WaveBank (%u loop)\n", j, loopcount);
			hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, volume);
	

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


	uint64_t componenthandle;
	uint64_t containerhandle;
	float amplitude;
	float durationseconds;
	float frequency;

	// map parsing
	size_t found;
	std::string first;
	std::string second;
	std::string third;
	std::string cont;


	wprintf(L"Listening SteamVR Events...\n");
	while (true)
	{

		Sleep(10);
		vr::VREvent_t vrEvent;


		while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vr::VREvent_t)))
		{

			std::string hapticmap;
			
			std::string ev = vr::VRSystem()->GetEventTypeNameFromEnum((vr::EVREventType)vrEvent.eventType);
			// wprintf(L"Event Received %hs\n",ev.c_str());

			if (ev == "VREvent_Input_HapticVibration")
			{
				componenthandle = vrEvent.data.hapticVibration.componentHandle;
				containerhandle = vrEvent.data.hapticVibration.containerHandle;
				amplitude = vrEvent.data.hapticVibration.fAmplitude;
				durationseconds = vrEvent.data.hapticVibration.fDurationSeconds;
				frequency = vrEvent.data.hapticVibration.fFrequency;
				wprintf(L"Haptic Vibration Detected\n");
				wprintf(L"Component: %I64u\n", componenthandle);
				wprintf(L"Container: %I64u\n", containerhandle);
				wprintf(L"Amplitude: %f\n", amplitude);
				wprintf(L"Duration (seconds): %f\n", durationseconds);
				wprintf(L"Frequency: %f\n", frequency);

				hapticmap = std::to_string(amplitude) + std::to_string(durationseconds) + std::to_string(frequency);

				if (maps[hapticmap].length() > 0)
				{
					
					std::cout << "map " << hapticmap << " found, using: " << maps[hapticmap] << '\n';

					found = maps[hapticmap].find(",");
					first = maps[hapticmap].substr(0, found);
					cont = maps[hapticmap].substr(found + 1, maps[hapticmap].length());
					found = cont.find(",");
					second = cont.substr(0, found);
					third = cont.substr(found + 1, cont.length());

					j = std::stoi(first);
					loopcount = std::stoi(second);
					volume = std::stof(third);

					hr = wb.GetMetadata(j, metadata);
					wprintf(L"Play entry %u from WaveBank, loop=%u, volume=%f\n", j, loopcount, volume);
					hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, j, loopcount, volume);


				}
				else {
					std::cout << "map not found, add " << hapticmap << " to .ini" << '\n';
					hr = wb.GetMetadata(defj, metadata);
					wprintf(L"USING DEFAULT -> Play entry %u from WaveBank, loop=%u, volume=%f\n", defj, defloopcount, defvolume);
					hr = PlayWaveFromWaveBank(pXAudio2.Get(), wb, defj, defloopcount, defvolume);

				}




			}
		}
	}

	
}



