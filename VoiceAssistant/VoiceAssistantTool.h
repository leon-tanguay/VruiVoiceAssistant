/***********************************************************************
VoiceAssistantTool - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md
section 2, and ../guide/RECODE_CHECKLIST.md item 12 -- this file and
VoiceAssistantNumberTool.h/.cpp differ from each other in exactly four
lines in v1; item 12 has the options for what to do about that).

Issuing the command -- v1's issueCommand() (VoiceAssistantTool.cpp) opens a
THROWAWAY pipe() for every single button press, writes the command string
into it, calls getCommandDispatcher().dispatchCommands(p[0]), then closes
both ends -- just to hand one known string to the SAME dispatcher call on
the SAME thread (a Vrui tool's buttonCallback() already runs on the main
thread; there is no cross-thread hand-off happening here at all, unlike
the vislet's worker-thread case in VoiceAssistant.h). That whole
pipe/write/dispatchCommands/close dance -- including the comment about
"the explicit branch that silences glibc's warn_unused_result on
write()" -- collapses to one line with <Misc/CommandDispatcher.h>'s
direct, no-fd call:
    Vrui::getCommandDispatcher().dispatchCommand(cmd,cmd+strlen(cmd));
See ../guide/RECODE_CHECKLIST.md item 12 and item 11's pipe note in
VoiceAssistant.h for the fuller picture (the vislet's OWN pipe, for the
worker-to-main case, has a different and better fix: a mutex-guarded
queue, not a direct call, because that one really does cross threads).
***********************************************************************/

#ifndef VRUI_TOOLS_VOICEASSISTANTTOOL_INCLUDED
#define VRUI_TOOLS_VOICEASSISTANTTOOL_INCLUDED

namespace Vrui {

// TODO: declare VoiceAssistantTool (../guide/RECODE_CHECKLIST.md item 12).

}

#endif
