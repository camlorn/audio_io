#include <audio_io/audio_io_private.hpp>
#include <audio_io/speex_resampler_config.h>
#include <algorithm>
#include <utility>
#include <math.h>
#include <string.h>
#include <stdio.h>


namespace audio_io {
namespace implementation {

Resampler::Resampler(int inputFrameCount, int inputChannels, int inputSr, int outputSr): input_frame_count(inputFrameCount), input_channels(inputChannels), input_sr(inputSr), output_sr(outputSr) {
	delta = (float)inputSr/(float)outputSr;
	spx_resampler = speex_resampler_init(inputChannels, inputSr, outputSr, 1, &spx_error);
}

void Resampler::read(float* source) {
	float* buff;
	if(done_queue.empty()) {
		buff = new float[input_frame_count*input_channels];
	}
	else {
		buff = done_queue.front();
		done_queue.pop_front();
	}
	std::copy(source, source+input_channels*input_frame_count, buff);
	queue.push_back(buff);
}

int Resampler::write(float* dest, int maxFrameCount) {
	int count = 0;
	float* buff;
	while(count < maxFrameCount && queue.empty() == false) {
		buff=queue.front();
		spx_uint32_t remainingInputFrames = input_frame_count-offset/input_channels;
		spx_uint32_t remainingOutputFrames = maxFrameCount-count;
		speex_resampler_process_interleaved_float(spx_resampler, buff+offset, &remainingInputFrames, dest, &remainingOutputFrames);
		//unfortunately, speex uses the lengths as out parameters.
		count +=remainingOutputFrames;
		offset += remainingInputFrames*input_channels;
		dest += remainingOutputFrames*input_channels;
		if(offset == input_frame_count*input_channels) {
			done_queue.push_back(queue.front());
			queue.pop_front();
			offset = 0;
		}
	}
	//Unfortunately, the speex resampler clips. We therefore attenuate by a very small amount.  This number was determined experimentaly.
	dest -=count*input_channels;
	for(int i=0; i < count*input_channels; i++) dest[i]*=0.94;
	return count;
}

int Resampler::estimateAvailableFrames() {
	float delta_rec = 1.0/delta;
	return queue.size()*delta_rec*input_frame_count;
}

}
}