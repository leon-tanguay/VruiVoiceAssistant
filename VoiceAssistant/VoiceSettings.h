/***********************************************************************
VoiceSettings - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md
section 2, and ../guide/style-example/Chirp/ChirpSettings.h for the pattern this
file follows -- a plain config struct with defaults in the constructor's
initialiser list). Not on ../guide/RECODE_CHECKLIST.md's numbered list, but every
tier depends on it, so it needs to exist before Json.cpp will link.
***********************************************************************/

#ifndef VOICEASSISTANT_VOICESETTINGS_INCLUDED
#define VOICEASSISTANT_VOICESETTINGS_INCLUDED

#include <string>

namespace VoiceAsst {

// TODO: declare VoiceSettings (see v1's VoiceAssistant/VoiceSettings.h for
// the full list of knobs: wake phrase, audio input, Vosk, Ollama, TTS,
// earcons, catalog path, activation mode -- but decide afresh, don't just
// copy it, whether every one of those still belongs here after the
// OpenAL-earcon removal, ../guide/DEPENDENCIES.md #11).

}

#endif
