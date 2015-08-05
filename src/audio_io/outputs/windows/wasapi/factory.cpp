//This is undocumented  COM magic.  Including this here should be sufficient for most of the library, but maybe not.
#include <initguid.h>
#include "wasapi.hpp"
#include <audio_io/private/audio_outputs.hpp>
#include <audio_io/private/single_threaded_apartment.hpp>
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

namespace audio_io {
namespace implementation {

WasapiOutputDeviceFactory::WasapiOutputDeviceFactory() {
	APARTMENTCALL(CoCreateInstance, CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
	//todo: errors.
	rescan();
}

WasapiOutputDeviceFactory::~WasapiOutputDeviceFactory() {
	if(enumerator) enumerator->Release();
}

std::vector<std::wstring> WasapiOutputDeviceFactory::getOutputNames() {
	return names;
}

std::vector<int> WasapiOutputDeviceFactory::getOutputMaxChannels() {
	return max_channels;
}

std::shared_ptr<OutputDevice> WasapiOutputDeviceFactory::createDevice(std::function<void(float*, int)> callback, int index, unsigned int channels, unsigned int sr, unsigned int blockSize, unsigned int mixAhead) {
	HRESULT res;
	IMMDevice* dev;
	if(index == -1) res = APARTMENTCALL(enumerator->GetDefaultAudioEndpoint, eRender, eMultimedia, &dev);
	else res = APARTMENTCALL(enumerator->GetDevice, ids_to_id_strings[index].c_str(), &dev);
	//Proper error handling!
	if(res != S_OK) return nullptr;
	return std::make_shared<WasapiOutputDevice>(callback, dev, blockSize, channels, sr, mixAhead);
}

unsigned int WasapiOutputDeviceFactory::getOutputCount() {
	return names.size();
}

std::string WasapiOutputDeviceFactory::getName() {
	return "Wasapi";
}

void WasapiOutputDeviceFactory::rescan() {
	names.clear();
	max_channels.clear();
	ids_to_id_strings.clear();
	IMMDeviceCollection *collection;
	enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
	UINT count;
	collection->GetCount(&count);
	for(UINT i = 0; i < count; i++) {
		std::wstring name;
		std::wstring identifierString;
		int channels;
		IMMDevice *device;
		collection->Item(i, &device);
		//We have to open the property store and use it to get the informationh.
		IPropertyStore *properties;
		device->OpenPropertyStore(STGM_READ, &properties);
		PROPVARIANT prop;
		properties->GetValue(PKEY_AudioEngine_DeviceFormat, &prop);
		WAVEFORMATEX *format = (WAVEFORMATEX*)prop.blob.pBlobData;
		channels = format->nChannels;
		//Easy string is the identifier.
		LPWSTR identifier;
		device->GetId(&identifier);
		identifierString = std::wstring(identifier);
		CoTaskMemFree(identifier);
		properties->GetValue(PKEY_DeviceInterface_FriendlyName, &prop);
		LPWSTR rawname = prop.pwszVal;
		name = std::wstring(rawname);
		this->max_channels.push_back(channels);
		names.push_back(name);
		ids_to_id_strings[i] = identifierString;
	}
}

OutputDeviceFactory* createWasapiOutputDeviceFactory() {
	//In order to determine if we have Wasapi, we attempt to open and close the default output device without error.
	try {
		SingleThreadedApartment sta;
		IMMDeviceEnumerator* enumerator = nullptr;
		IMMDevice *default_device = nullptr;
		IAudioClient* client = nullptr;
		auto res = sta.callInApartment(CoCreateInstance, CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
		if(IS_ERROR(res)) return nullptr;
		//Attempt to get the default device.
		res = APARTMENTCALL(enumerator->GetDefaultAudioEndpoint, eRender, eMultimedia, &default_device);
		if(IS_ERROR(res)) {
			enumerator->Release();
			return nullptr;
		}
		res = APARTMENTCALL(default_device->Activate, IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);
		if(IS_ERROR(res)) {
			default_device->Release();
			enumerator->Release();
			return nullptr;
		}
		//We now have an IAudioClient.  We can attempt to initialize it with the default mixer format in shared mode, which is supposed to always be accepted.
		WAVEFORMATEX *format = nullptr;
		res = APARTMENTCALL(client->GetMixFormat, &format);
		if(IS_ERROR(res)) {
			client->Release();
			default_device->Release();
			enumerator->Release();
			return nullptr;
		}
		//1e6 nanoseconds in a millisecond.
		//We don't request a specific buffer length, we just want to know if we can open and otherwise don't care.
		res = APARTMENTCALL(client->Initialize, AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, format, NULL);
		if(IS_ERROR(res)) {
			client->Release();
			default_device->Release();
			enumerator->Release();
			return nullptr;
		}
		client->Release();
		default_device->Release();
		enumerator->Release();
		return new WasapiOutputDeviceFactory();
	}
	catch(...) {
		return nullptr;
	}
	return nullptr;
}

}
}