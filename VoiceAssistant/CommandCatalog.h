/***********************************************************************
CommandCatalog - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 4 for what this file is
supposed to do and what's wrong with v1's version).

Loading VoiceAssistant-catalog.json: read the comment at the top of
Json.h first. IO::JsonSource("VoiceAssistant-catalog.json").parseEntity()
gets you the root IO::JsonPointer; IO::getObjectProperty(root,"commands")
+ IO::getArray(...) walks the "commands" array the same way v1's hand-rolled
Json did, just with Vrui's own type. Don't include "Json.h" here.
***********************************************************************/

#ifndef VOICEASSISTANT_COMMANDCATALOG_INCLUDED
#define VOICEASSISTANT_COMMANDCATALOG_INCLUDED

namespace VoiceAsst {

// TODO: declare CommandCatalog (../guide/RECODE_CHECKLIST.md item 4).

}

#endif
