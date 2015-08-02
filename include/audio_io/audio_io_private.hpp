#pragma once
#include "audio_io.hpp"
#include <speex_resampler_cpp.hpp>
#include <vector>
#include <list>
#include <set>
#include <memory>
#include <utility>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

//The details in this file are to be considered private.
//Do not include or use this file in public code directly.

namespace audio_io {
namespace implementation {

class OutputDeviceImplementation: public OutputDevice {
	protected:
	OutputDeviceImplementation() = default;
	virtual ~OutputDeviceImplementation();
	//function parameters: output buffer, number of channels to write.
	virtual void init(std::function<void(float*, int)> getBuffer, unsigned int inputBufferFrames, unsigned int inputBufferSr, unsigned int channels, unsigned int outputSr, unsigned int mixAhead); //second step fn initialization. We can't just fall through to the constructor.
	virtual void start(); //final step in initialization via subclasses: starts the background thread.
	virtual void stop(); //stop the output.
	virtual void zeroOrNextBuffer(float* where);
	virtual void mixingThreadFunction();
	unsigned int channels = 0;
	unsigned int mix_ahead = 0;
	unsigned int input_buffer_size, output_buffer_size, input_buffer_frames, output_buffer_frames;
	unsigned int input_sr = 0;
	unsigned int output_sr = 0;
	bool is_resampling = false;
	bool should_apply_mixing_matrix = false;
	unsigned int next_output_buffer = 0;
	unsigned int callback_buffer_index = 0;

	float** buffers = nullptr;
	std::atomic<int>* buffer_statuses = nullptr;
	std::atomic_flag mixing_thread_continue;
	std::thread mixing_thread;
	std::function<void(float*, int)> get_buffer;
	bool started = false;
	friend class OutputDeviceFactoryImplementation;
};

class OutputDeviceFactoryImplementation: public OutputDeviceFactory {
	public:
	virtual ~OutputDeviceFactoryImplementation();
	//these two are overridden here, others come later.
	virtual unsigned int getOutputCount();
	virtual std::string getName();
	protected:
	std::vector<std::weak_ptr<OutputDeviceImplementation>> created_devices;
	int output_count = 0;
};

typedef OutputDeviceFactory* (*OutputDeviceFactoryCreationFunction)();
OutputDeviceFactory* createWinmmOutputDeviceFactory();

}
}