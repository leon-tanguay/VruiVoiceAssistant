/***********************************************************************
SttEngine - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 6 for what this file is
supposed to do and what's wrong with v1's version).

Parsing Vosk's result JSON (vosk_recognizer_result() returns a JSON C
string, e.g. {"text":"reset the view"} or the grammar/confidence variant):
read the comment at the top of Json.h first. Same IO::FixedMemoryFile +
IO::JsonSource + IO::getObjectProperty()/IO::getString() approach as
Resolver.h uses for Ollama's replies -- not the old hand-rolled Json class.
***********************************************************************/

#ifndef VOICEASSISTANT_STTENGINE_INCLUDED
#define VOICEASSISTANT_STTENGINE_INCLUDED

namespace VoiceAsst {

// TODO: declare SttEngine (../guide/RECODE_CHECKLIST.md item 6).

}

#endif
