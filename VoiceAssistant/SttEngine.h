/***********************************************************************
SttEngine - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 6 for what this file is
supposed to do and what's wrong with v1's version).

Parsing Vosk's result JSON (vosk_recognizer_result() returns a JSON C
string, e.g. {"text":"reset the view"} or the grammar/confidence variant):
see ../guide/RECODE_CHECKLIST.md item 1. Same IO::FixedMemoryFile +
IO::JsonSource + IO::getObjectProperty()/IO::getString() approach as
Resolver.h uses for Ollama's replies -- not the old hand-rolled Json class.
***********************************************************************/

#ifndef VOICEASSISTANT_STTENGINE_INCLUDED
#define VOICEASSISTANT_STTENGINE_INCLUDED

#include <Misc/SelfDestructPointer.h>
#include <Misc/SizedTypes.h>

#include <string>
#include <vector>

// Opaque Vosk C types, defined in vosk_api.h
// Lets us not need to include all of vosk.h in header, just recognises that Vosk exists
struct VoskModel;
struct VoskRecognizer;

namespace VoiceAsst
{

class SttEngine
{
	private:
	struct ModelHandle // Owns a VoskModel*; frees it on destruction
	{
		VoskModel* model;

		ModelHandle(const char* modelPath, bool verbose); // Loads the model; throws std::runtime_error on failure
		~ModelHandle(void); // Calls vosk_model_free

		private:
		ModelHandle(const ModelHandle& source); // Prohibit copy constructor
		ModelHandle& operator=(const ModelHandle& source); // Prohibit assignment
	};

	struct RecognizerHandle // Owns a VoskRecognizer*; frees it on destruction
	{
		VoskRecognizer* recognizer;

		RecognizerHandle(VoskModel* model, float sampleRate); // Null recognizer on failure (does not throw); SttEngine's constructor checks for this and throws
		~RecognizerHandle(void); // Calls vosk_recognizer_free if non-null

		private:
		RecognizerHandle(const RecognizerHandle& source); // Prohibit copy constructor
		RecognizerHandle& operator=(const RecognizerHandle& source); // Prohibit assignment
	};

	unsigned int channels; // Microphone channel count, fixed at construction, for mono mixdown in feed()

	ModelHandle model; // The loaded Vosk model, shared by every transcription

	RecognizerHandle recognizer; // One recognizer for the engine's lifetime, reset by begin() for each utterance; never null once constructed

	public:
	SttEngine(const char* modelPath, bool verbose, unsigned int frameRate, unsigned int sChannels); // Loads the Vosk model from the given folder and creates a recognizer for the microphone's frame rate and channels; throws std::runtime_error if either fails
	void feed(const Misc::SInt16* samples, size_t numSamples); // Adds interleaved samples to the current utterance
	void begin(); // Starts a new utterance, resetting the recognizer
	std::string finish(void); // Ends the utterance and returns the text, or "" if nothing was recognized
};

}

#endif
