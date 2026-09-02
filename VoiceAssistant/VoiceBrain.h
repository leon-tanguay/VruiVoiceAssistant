/***********************************************************************
VoiceBrain - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 10 for what this file is
supposed to do and what's wrong with v1's version).

TWO TRAPS, checked and confirmed -- do NOT "fix" either of these:

1. Timing (lastSpeakEnd, the wake cooldown, commandSilenceTimeout, etc.).
   v1's monotonic() (a free function, top of VoiceBrain.cpp) hand-rolls
   clock_gettime(CLOCK_MONOTONIC,&ts) -- and that is CORRECT, not janky.
   Vrui's own <Misc/Time.h> (Misc::Time::now()) looks like the obvious
   "use the toolkit's version" swap, but its now() calls gettimeofday(),
   which is WALL-CLOCK time -- it can jump (NTP sync, manual clock change)
   and using it to measure "how many seconds since the assistant stopped
   speaking" would be a real, hard-to-reproduce bug. Keep monotonic() as
   a hand-rolled clock_gettime(CLOCK_MONOTONIC,...) wrapper; this is one
   of the few places v1 was already doing the more-correct thing than the
   Vrui class with the matching name.

2. The worker thread's shape. Threads::RunLoopThread + Threads::RunLoop's
   UserSignal (<Threads/RunLoopThread.h>, <Threads/RunLoop.h>) is a real,
   general-purpose "background thread running an event loop, with a
   signal() callable from any thread to wake it" mechanism -- genuinely
   relevant-SOUNDING for a worker thread the main thread needs to poke.
   But it's an I/O-multiplexing reactor (select/poll over many fds/timers/
   signal handlers dispatching to different handlers) built for something
   that watches MANY asynchronous sources at once. VoiceBrain is the
   opposite shape: one long BLOCKING sequential pipeline (block on mic
   read, block on Vosk, block on curl to Ollama, block on writing to
   Piper) running alone on its own thread. Forcing that into an
   event-driven reactor would need turning every blocking call into a
   registered callback -- more complex, not less. Plain Threads::Thread +
   Threads::MutexCond (the style-example/Chirp/Worker.cpp pattern) is the
   right-sized tool here; RunLoopThread is worth knowing about for a
   FUTURE piece of this project that genuinely watches multiple async
   sources (arguably the vislet's own frame()/command-queue interplay
   might be one, if it ever gets more complex than it is in item 11).

ROBUSTNESS: read ../guide/CODE_REVIEW.md finding #2 before writing stop().
v1's stop() sets running=false and interruptRequested=true, then calls
thread.join() -- but those are flags the worker loop only checks BETWEEN
blocking calls. If the worker is currently blocked inside
OllamaClient::post() (up to a 60 s libcurl timeout by default), a flag
does nothing to unblock it, and thread.join() -- called synchronously
from VoiceAssistant::disable() on the MAIN thread -- hangs the entire
Vrui application for however long is left of that timeout. Setting a
flag can only ever cancel a blocking call from the OUTSIDE if the thing
actually being blocked on has its own cancellation hook (for libcurl:
a progress/xfer callback via CURLOPT_XFERINFOFUNCTION that can return
non-zero to abort mid-transfer). Design stop() around interrupting the
ACTUAL blocking resource, not just a flag the loop reads once it's
already back from the blocking call.
***********************************************************************/

#ifndef VOICEASSISTANT_VOICEBRAIN_INCLUDED
#define VOICEASSISTANT_VOICEBRAIN_INCLUDED

namespace VoiceAsst {

// TODO: declare VoiceBrain (../guide/RECODE_CHECKLIST.md item 10).

}

#endif
