#define WIN32_LEAN_AND_MEAN
#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>


#include <wrl\client.h>


#include "WaveBankReader.h"

#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"runtimeobject.lib")
#include <Windows.Devices.Enumeration.h>
#include <wrl.h>
#include <ppltasks.h>


using namespace DirectX;
using Microsoft::WRL::ComPtr;


struct AudioDevice
{
	std::wstring deviceId;
	std::wstring description;
};

HRESULT EnumerateAudio(_In_ IXAudio2* pXaudio2, _Inout_ std::vector<AudioDevice>& list);


//--------------------------------------------------------------------------------------
// Enumerate audio end-points
//--------------------------------------------------------------------------------------
HRESULT EnumerateAudio(_In_ IXAudio2* pXaudio2, _Inout_ std::vector<AudioDevice>& list)
{
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)

	UNREFERENCED_PARAMETER(pXaudio2);

#if defined(__cplusplus_winrt )

	// Enumerating with WinRT using C++/CX
	using namespace concurrency;
	using Windows::Devices::Enumeration::DeviceClass;
	using Windows::Devices::Enumeration::DeviceInformation;
	using Windows::Devices::Enumeration::DeviceInformationCollection;

	auto operation = DeviceInformation::FindAllAsync(DeviceClass::AudioRender);

	auto task = create_task(operation);

	task.then([&list](DeviceInformationCollection^ devices)
	{
		for (unsigned i = 0; i < devices->Size; ++i)
		{
			using Windows::Devices::Enumeration::DeviceInformation;

			DeviceInformation^ d = devices->GetAt(i);

			AudioDevice device;
			device.deviceId = d->Id->Data();
			device.description = d->Name->Data();
			list.emplace_back(device);
		}
	});

	task.wait();

	if (list.empty())
		return S_FALSE;

#else

	// Enumerating with WinRT using WRL
	using namespace Microsoft::WRL;
	using namespace Microsoft::WRL::Wrappers;
	using namespace ABI::Windows::Foundation;
	using namespace ABI::Windows::Foundation::Collections;
	using namespace ABI::Windows::Devices::Enumeration;

	RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
	HRESULT hr = initialize;
	if (FAILED(hr))
		return hr;

	Microsoft::WRL::ComPtr<IDeviceInformationStatics> diFactory;
	hr = ABI::Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(), &diFactory);
	if (FAILED(hr))
		return hr;

	Event findCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));
	if (!findCompleted.IsValid())
		return HRESULT_FROM_WIN32(GetLastError());

	auto callback = Callback<IAsyncOperationCompletedHandler<DeviceInformationCollection*>>(
		[&findCompleted, list](IAsyncOperation<DeviceInformationCollection*>* aDevices, AsyncStatus status) -> HRESULT
	{
		SetEvent(findCompleted.Get());
		return S_OK;
	});

	ComPtr<IAsyncOperation<DeviceInformationCollection*>> operation;
	hr = diFactory->FindAllAsyncDeviceClass(DeviceClass_AudioRender, operation.GetAddressOf());
	if (FAILED(hr))
		return hr;

	operation->put_Completed(callback.Get());

	WaitForSingleObject(findCompleted.Get(), INFINITE);

	ComPtr<IVectorView<DeviceInformation*>> devices;
	operation->GetResults(devices.GetAddressOf());

	unsigned int count = 0;
	hr = devices->get_Size(&count);
	if (FAILED(hr))
		return hr;

	if (!count)
		return S_FALSE;

	for (unsigned int j = 0; j < count; ++j)
	{
		ComPtr<IDeviceInformation> deviceInfo;
		hr = devices->GetAt(j, deviceInfo.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			HString id;
			deviceInfo->get_Id(id.GetAddressOf());

			HString name;
			deviceInfo->get_Name(name.GetAddressOf());

			AudioDevice device;
			device.deviceId = id.GetRawBuffer(nullptr);
			device.description = name.GetRawBuffer(nullptr);
			list.emplace_back(device);
		}
	}

	return S_OK;

#endif 

#else // _WIN32_WINNT < _WIN32_WINNT_WIN8

	// Enumerating with XAudio 2.7
	UINT32 count = 0;
	HRESULT hr = pXaudio2->GetDeviceCount(&count);
	if (FAILED(hr))
		return hr;

	if (!count)
		return S_FALSE;

	list.reserve(count);

	for (UINT32 j = 0; j < count; ++j)
	{
		XAUDIO2_DEVICE_DETAILS details;
		hr = pXaudio2->GetDeviceDetails(j, &details);
		if (SUCCEEDED(hr))
		{
			AudioDevice device;
			device.deviceId = details.DeviceID;
			device.description = details.DisplayName;
			list.emplace_back(device);
		}
	}

#endif

	return S_OK;
}





//--------------------------------------------------------------------------------------
// Helper function to try to find the location of a media file
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT FindMediaFileCch(WCHAR* strDestPath, int cchDest, LPCWSTR strFilename)
{
	bool bFound = false;

	if (!strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10)
		return E_INVALIDARG;

	// Get the exe name, and exe path
	WCHAR strExePath[MAX_PATH] = { 0 };
	WCHAR strExeName[MAX_PATH] = { 0 };
	WCHAR* strLastSlash = nullptr;
	GetModuleFileName(nullptr, strExePath, MAX_PATH);
	strExePath[MAX_PATH - 1] = 0;
	strLastSlash = wcsrchr(strExePath, TEXT('\\'));
	if (strLastSlash)
	{
		wcscpy_s(strExeName, MAX_PATH, &strLastSlash[1]);

		// Chop the exe name from the exe path
		*strLastSlash = 0;

		// Chop the .exe from the exe name
		strLastSlash = wcsrchr(strExeName, TEXT('.'));
		if (strLastSlash)
			*strLastSlash = 0;
	}

	wcscpy_s(strDestPath, cchDest, strFilename);
	if (GetFileAttributes(strDestPath) != 0xFFFFFFFF)
		return S_OK;

	// Search all parent directories starting at .\ and using strFilename as the leaf name
	WCHAR strLeafName[MAX_PATH] = { 0 };
	wcscpy_s(strLeafName, MAX_PATH, strFilename);

	WCHAR strFullPath[MAX_PATH] = { 0 };
	WCHAR strFullFileName[MAX_PATH] = { 0 };
	WCHAR strSearch[MAX_PATH] = { 0 };
	WCHAR* strFilePart = nullptr;

	GetFullPathName(L".", MAX_PATH, strFullPath, &strFilePart);
	if (!strFilePart)
		return E_FAIL;

	while (strFilePart && *strFilePart != '\0')
	{
		swprintf_s(strFullFileName, MAX_PATH, L"%s\\%s", strFullPath, strLeafName);
		if (GetFileAttributes(strFullFileName) != 0xFFFFFFFF)
		{
			wcscpy_s(strDestPath, cchDest, strFullFileName);
			bFound = true;
			break;
		}

		swprintf_s(strFullFileName, MAX_PATH, L"%s\\%s\\%s", strFullPath, strExeName, strLeafName);
		if (GetFileAttributes(strFullFileName) != 0xFFFFFFFF)
		{
			wcscpy_s(strDestPath, cchDest, strFullFileName);
			bFound = true;
			break;
		}

		swprintf_s(strSearch, MAX_PATH, L"%s\\..", strFullPath);
		GetFullPathName(strSearch, MAX_PATH, strFullPath, &strFilePart);
	}
	if (bFound)
		return S_OK;

	// On failure, return the file as the path but also return an error code
	wcscpy_s(strDestPath, cchDest, strFilename);

	return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}





//--------------------------------------------------------------------------------------
// Name: PlayWaveFromWaveBank
// Desc: Plays a wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT PlayWaveFromWaveBank(IXAudio2* pXaudio2, WaveBankReader& wb, uint32_t index, uint32_t loopcount, float volume)
{
	if (index >= wb.Count())
		return E_INVALIDARG;

	uint8_t waveFormat[64];
	auto pwfx = reinterpret_cast<WAVEFORMATEX*>(&waveFormat);

	HRESULT hr = wb.GetFormat(index, pwfx, 64);
	if (FAILED(hr))
		return hr;

	const uint8_t* waveData = nullptr;
	uint32_t waveSize;

	hr = wb.GetWaveData(index, &waveData, waveSize);
	if (FAILED(hr))
		return hr;

	WaveBankReader::Metadata metadata;
	hr = wb.GetMetadata(index, metadata);
	if (FAILED(hr))
		return hr;

	//
	// Play the wave using a XAudio2SourceVoice
	//

	// Create the source voice
	IXAudio2SourceVoice* pSourceVoice;
	if (FAILED(hr = pXaudio2->CreateSourceVoice(&pSourceVoice, pwfx)))
	{
		wprintf(L"Error %#X creating source voice\n", hr);
		return hr;
	}

	// Submit the wave sample data using an XAUDIO2_BUFFER structure
	XAUDIO2_BUFFER buffer = { 0 };
	buffer.pAudioData = waveData;
	buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
	buffer.AudioBytes = waveSize;

	buffer.LoopCount = loopcount;



	const uint32_t* seekTable;
	uint32_t seekTableCount;
	uint32_t tag;
	hr = wb.GetSeekTable(index, &seekTable, seekTableCount, tag);

	if (seekTable)
	{
		if (tag == WAVE_FORMAT_WMAUDIO2 || tag == WAVE_FORMAT_WMAUDIO3)
		{
#if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) || (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/ )
			XAUDIO2_BUFFER_WMA xwmaBuffer = { 0 };
			xwmaBuffer.pDecodedPacketCumulativeBytes = seekTable;
			xwmaBuffer.PacketCount = seekTableCount;
			if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&buffer, &xwmaBuffer)))
			{
				wprintf(L"Error %#X submitting source buffer (xWMA)\n", hr);
				pSourceVoice->DestroyVoice();
				return hr;
			}
#else
			wprintf(L"This platform does not support xWMA\n");
			pSourceVoice->DestroyVoice();
			return hr;
#endif
		}
		else if (tag == 0x166 /* WAVE_FORMAT_XMA2 */)
		{
			wprintf(L"This platform does not support XMA2\n");
			pSourceVoice->DestroyVoice();
			return hr;
		}
	}
	else if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&buffer)))
	{
		wprintf(L"Error %#X submitting source buffer\n", hr);
		pSourceVoice->DestroyVoice();
		return hr;
	}

	hr = pSourceVoice->SetVolume(volume);

	hr = pSourceVoice->Start(0);

	// Let the sound play
	BOOL isRunning = TRUE;
	while (SUCCEEDED(hr) && isRunning)
	{
		XAUDIO2_VOICE_STATE state;
		pSourceVoice->GetState(&state);
		isRunning = (state.BuffersQueued > 0) != 0;

		// Wait till the escape key is pressed
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		Sleep(10);
	}

	// Wait till the escape key is released
	while (GetAsyncKeyState(VK_ESCAPE))
		Sleep(10);

	pSourceVoice->DestroyVoice();

	return hr;
}

