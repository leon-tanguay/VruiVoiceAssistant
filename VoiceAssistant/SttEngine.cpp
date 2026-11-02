/***********************************************************************
SttEngine - See SttEngine.h.
***********************************************************************/
#include "SttEngine.h"

#include <IO/File.h>
#include <IO/FixedMemoryFile.h>
#include <IO/JsonEntityTypes.h>
#include <IO/JsonSource.h>

#include <stdexcept> // Contains std::runtime_error
#include <string.h> // strlen, memcpy

#include <vosk_api.h>

namespace VoiceAsst
{

SttEngine::ModelHandle::ModelHandle(const char* modelPath, bool verbose) :
	model(nullptr)
{
	vosk_set_log_level(verbose ? 0 : -1); // Must come before loading: the load is what prints the log. Applies to all of Vosk, not just this model
	model = vosk_model_new(modelPath);
	if (model == nullptr)
		throw std::runtime_error(std::string("SttEngine: failed to load Vosk model from ") + modelPath);
}

SttEngine::ModelHandle::~ModelHandle(void)
{
	vosk_model_free(model);
}

SttEngine::RecognizerHandle::RecognizerHandle(VoskModel* model, float sampleRate) :
	recognizer(vosk_recognizer_new(model, sampleRate))
{
}

SttEngine::RecognizerHandle::~RecognizerHandle(void)
{
	if (recognizer != nullptr)
		vosk_recognizer_free(recognizer);
}

// Initializes the SttEngine with a Vosk model and prepares for recognition (throws with bad model path or recognizer)
SttEngine::SttEngine(const char* modelPath, bool verbose, unsigned int frameRate, unsigned int sChannels) :
	channels(sChannels), model(modelPath, verbose), recognizer(model.model, float(frameRate))
{
	if (recognizer.recognizer == nullptr)
		throw std::runtime_error("SttEngine: failed to create Vosk recognizer at " + std::to_string(frameRate) + " Hz");
}

// Starts a new utterance, resetting the recognizer
void SttEngine::begin()
{
	vosk_recognizer_reset(recognizer.recognizer);
}

// Feed samples to the recognizer, converting to mono if necessary
void SttEngine::feed(const Misc::SInt16* samples, size_t numSamples)
{
	const Misc::SInt16* audio = samples;
	size_t audioSize = numSamples;
	std::vector<Misc::SInt16> mono;
	if (channels > 1)
	{
		const size_t numFrames = numSamples / channels;
		mono.resize(numFrames);
		for (size_t frame = 0; frame < numFrames; ++frame)
		{
			int sum = 0;
			for (unsigned int channel = 0; channel < channels; ++channel)
				sum += samples[frame * channels + channel];
			mono[frame] = Misc::SInt16(sum / int(channels));
		}
		audio = mono.data();
		audioSize = numFrames;
	}

	vosk_recognizer_accept_waveform_s(recognizer.recognizer, audio, int(audioSize));
}

std::string SttEngine::finish(void)
{
	const char* json = vosk_recognizer_final_result(recognizer.recognizer);
	std::string text;
	if (json != nullptr)
	{
		try
		{
			// Copy Vosk's result into a memory file Vrui's JSON parser can read
			const size_t jsonSize = strlen(json);
			IO::FixedMemoryFile* jsonFile = new IO::FixedMemoryFile(jsonSize);
			IO::FilePtr jsonFilePtr(jsonFile);
			memcpy(jsonFile->getMemory(), json, jsonSize);

			// Parse it and pull out the "text" property
			IO::JsonSource jsonSource(jsonFilePtr);
			IO::JsonPointer root = jsonSource.parseEntity();
			text = IO::getString(IO::getObjectProperty(root, "text"));
		}
		catch (const std::runtime_error&)
		{
		}
	}

	return text;
}

}