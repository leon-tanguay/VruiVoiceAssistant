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

namespace
{ // Everything inside ONLY visible in this c++ file

struct RecognizerHandle // Owns a VoskRecognizer*; frees it on destruction
{
	VoskRecognizer* recognizer;

	RecognizerHandle(VoskModel* model, float sampleRate) :
		recognizer(vosk_recognizer_new(model, sampleRate))
	{
	}
	~RecognizerHandle(void)
	{
		if (recognizer != nullptr)
			vosk_recognizer_free(recognizer);
	}

	private:
	RecognizerHandle(const RecognizerHandle& source); // Prohibit copy constructor
	RecognizerHandle& operator=(const RecognizerHandle& source); // Prohibit assignment
};

}

SttEngine::ModelHandle::ModelHandle(const char* modelPath) :
	model(vosk_model_new(modelPath))
{
	if (model == nullptr)
	{
		throw std::runtime_error(std::string("SttEngine: failed to load Vosk model from ") + modelPath);
	}
}

SttEngine::ModelHandle::~ModelHandle(void)
{
	vosk_model_free(model);
}

SttEngine::SttEngine(const char* modelPath) :
	model(modelPath)
{
}

std::string SttEngine::transcribe(const std::vector<Misc::SInt16>& samples, unsigned int frameRate, unsigned int channels) const
{
	// Reject bad input
	if (samples.empty() || frameRate == 0 || channels == 0)
	{
		return std::string();
	}

	// Mix audio down to mono
	std::vector<Misc::SInt16> mono;
	const std::vector<Misc::SInt16>* audio = &samples;
	if (channels > 1)
	{
		const size_t numFrames = samples.size() / channels;
		mono.resize(numFrames);
		for (size_t frame = 0; frame < numFrames; ++frame)
		{
			int sum = 0;
			for (unsigned int channel = 0; channel < channels; ++channel)
				sum += samples[frame * channels + channel];
			mono[frame] = Misc::SInt16(sum / int(channels));
		}
		audio = &mono;
	}

	// Run recognizer
	RecognizerHandle recognizer(model.model, float(frameRate));
	if (recognizer.recognizer == nullptr)
		return std::string();

	vosk_recognizer_accept_waveform_s(recognizer.recognizer, audio->data(), int(audio->size()));
	const char* json = vosk_recognizer_final_result(recognizer.recognizer);

	// Return results before recognizer dies
	if (json == nullptr)
		return std::string();

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
		return IO::getString(IO::getObjectProperty(root, "text"));
	}
	catch (const std::runtime_error& err)
	{
		return std::string();
	}
}

}