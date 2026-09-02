/***********************************************************************
AudioOut - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 2 for what this file is
supposed to do and what's wrong with v1's version).

Checked, doesn't fit -- keep raw PulseAudio (pa_simple), don't switch to
Vrui's own sound-playback class: <Sound/SoundPlayer.h>, Sound::SoundPlayer
is real, but (1) on Linux it's ALSA-only (Sound::Linux::ALSAPCMDevice
under the hood, no PulseAudio path), which is the exact problem
AudioCapture.h's own device-fallback comment exists to work around under
WSLg, and (2) it plays from a WAV FILE on disk (constructor takes a file
name), not an in-memory PCM buffer -- using it would mean writing every
chime and TTS utterance to a temp WAV first, reintroducing the kind of
temp-file dance item 8 (Tts.cpp) is busy deleting. Vrui's own class
doesn't fit this project's two actual constraints (WSLg audio routing,
in-memory synthesized/streamed audio); pa_simple does.
***********************************************************************/

#ifndef VOICEASSISTANT_AUDIOOUT_INCLUDED
#define VOICEASSISTANT_AUDIOOUT_INCLUDED

namespace VoiceAsst {

// TODO: declare AudioOut (../guide/RECODE_CHECKLIST.md item 2).

}

#endif
