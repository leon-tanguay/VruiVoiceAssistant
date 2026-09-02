/***********************************************************************
VoiceAssistantNumberTool - TODO: write the banner comment (see
../guide/STYLE_GUIDE.md section 2, and ../guide/RECODE_CHECKLIST.md item 13 -- read item 12
first, since these two tools are near-duplicates of each other and the
recommendation is a shared base class between them).

Same pipe-vs-direct-call note as VoiceAssistantTool.h applies here too --
whatever this tool does to issue "voiceAssistant.listenNumber" wants
Vrui::getCommandDispatcher().dispatchCommand(cmd,cmd+strlen(cmd)), not a
throwaway pipe(). Read VoiceAssistantTool.h's comment for the full
reasoning; if these two tools end up sharing a base class (item 12's
question), this is exactly the kind of one-line logic that belongs there
once instead of twice.
***********************************************************************/

#ifndef VRUI_TOOLS_VOICEASSISTANTNUMBERTOOL_INCLUDED
#define VRUI_TOOLS_VOICEASSISTANTNUMBERTOOL_INCLUDED

namespace Vrui {

// TODO: declare VoiceAssistantNumberTool (../guide/RECODE_CHECKLIST.md item 13).

}

#endif
