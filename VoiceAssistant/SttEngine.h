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

#include <string>
#include <vector>
#include <Misc/SizedTypes.h>

// Opaque Vosk C types, defined in vosk_api.h
// Lets us not need to include all of vosk.h in header, just recognises that Vosk exists
struct VoskModel;

namespace VoiceAsst
{

class SttEngine
{
	private:
	struct ModelHandle // Owns a VoskModel*; frees it on destruction
	{
		VoskModel* model;

		ModelHandle(const char* modelPath); // Loads the model; throws std::runtime_error on failure
		~ModelHandle(void); // Calls vosk_model_free

		private:
		ModelHandle(const ModelHandle& source); // Prohibit copy constructor
		ModelHandle& operator=(const ModelHandle& source); // Prohibit assignment
	};

	ModelHandle model; // The loaded Vosk model, shared by every transcription

	public:
	SttEngine(const char* modelPath); // Loads the Vosk model from the given folder; throws std::runtime_error on failure
	std::string transcribe(const std::vector<Misc::SInt16>& samples, unsigned int frameRate, unsigned int channels) const; // Returns the spoken text, or "" if nothing was recognized
};

}

#endif
