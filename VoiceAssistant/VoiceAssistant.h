/***********************************************************************
VoiceAssistant - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md
section 2, and ../guide/RECODE_CHECKLIST.md item 11 for what this file is
supposed to do and what's wrong with v1's version -- most notably the
duplicated orb-state fields. CORRECTED: the OpenAL earcon path is NOT
dead code to delete -- see the audio comment below and
../guide/DEPENDENCIES.md #11, which has been rewritten).
***********************************************************************/

/***********************************************************************
DRAWING THE HALO (current design, replaces the earlier orb plans):
feedback is a glow along the edges of the user's view, colored and
pulsing by state. VoiceAssistant::HaloNode (defined in the .cpp) is a
SceneGraph::GraphNode in Vrui's physical-space scene graph, drawn only in
the transparent pass. Each call it draws around the visible part of the
current window's screen: getDisplayState(...).screen and
getScreenTransformation() give the screen's frame, and
window->getPanRect() gives the part of it actually visible (on the
desktop the window is a panning view into a wider screen; on an HMD the
pan rect is the full screen). Look/feel settings live in
VoiceAssistantFactory and are read from Vrui.cfg.

Still to check: on a real headset, whether the lenses hide the outermost
part of the glow (fix: thicker halo, or an inset setting), and whether
scene objects in front of the screen plane cover it (fix: glDepthRange,
as in Vrui's MouseCursorFaker.cpp).
***********************************************************************/

/***********************************************************************
AUDIO -- sound should come from the assistant's location, not play flat.
Direct guidance: "sound output from an assistant should come from the 3D
location of the assistant's graphical representation."

NEW OPEN QUESTION since the orb became a halo: the halo surrounds the
whole view, so it has no single 3D point to play sound from. Ask Oliver
where the assistant's voice should come from now (e.g. a fixed point just
in front of the head). The notes below were written for the orb; "the
orb's position" means whatever point that answer gives. This REVERSES an
earlier recommendation in this guide to delete v1's OpenAL earcon path
as dead code -- it is not dead code to remove, it is the right design,
currently non-functional only because this dev box's installed Vrui was
built with SYSTEM_HAVE_OPENAL=0 (see ../guide/DEPENDENCIES.md #11,
rewritten). Keep the ALObject/sound()/initContext() machinery.

One real gap in v1's own version, independent of the OpenAL-disabled
issue: initContext()/sound() (v1's VoiceAssistant.cpp lines 650-708) set
up AL sources and buffers and call alSourcePlay() -- but NEVER call
alSourcefv(source,AL_POSITION,...) to place the source anywhere. Even
with OpenAL enabled, v1's earcons would have played at OpenAL's default
(the origin), not at the orb. Fix: set AL_POSITION on each source, every
frame the orb's head-relative position could have changed, using the
SAME point computed for drawing the orb above -- the sound and the
graphic should visibly/audibly be the same place.

Open question worth asking your boss directly, not guessing at: does this
apply only to the short earcon chime (a one-shot buffer, straightforward
to position), or also to the TTS speech output itself? Speech is a long,
streamed signal (Piper's --output_raw PCM, per Tts.cpp's plan in
../guide/RECODE_CHECKLIST.md item 8) -- spatializing a STREAM through
OpenAL means queuing buffers as they arrive (alSourceQueueBuffers /
alSourceUnqueueBuffers) rather than the one-shot alBufferData a fixed
earcon WAV uses, which is a materially bigger piece of work than
repositioning a chime. Worth confirming scope before Milestone 8 rather
than discovering it there.
***********************************************************************/

/***********************************************************************
Running a command without a pipe -- references for the worker-to-main
command hand-off (Milestone 3 in ../guide/BUILD_ORDER.md; see
../guide/RECODE_CHECKLIST.md item 11).

v1's commandPipe[2] opens a REAL OS pipe (pipe()+fcntl(O_NONBLOCK)) just
to move a resolved command STRING from the VoiceBrain worker thread to
the main thread, which then calls
getCommandDispatcher().dispatchCommands(commandPipe[0]) every single
frame() regardless (it's never registered with any event-loop/select
mechanism -- frame() already runs continuously while the orb animates,
via Vrui::requestUpdate()). So the pipe isn't buying a wakeup; it's just
being used as a thread-safe string mailbox, the hard way.

<Misc/CommandDispatcher.h> already has the direct call:
    void dispatchCommand(const char* begin,const char* end); // one command, no fd
(distinct from dispatchCommands(int fd), plural, which reads and
dispatches every complete command sitting in a real file/pipe -- the
right tool when the source genuinely IS an external fd, the wrong tool
for moving one string between two threads you already control).

So: no pipe() / fcntl() / write() / read() / close() at all. Use exactly
the pattern already built in ../guide/style-example/Chirp/Worker.cpp +
main.cpp's Console: a Threads::Mutex-guarded std::vector<std::string> of
pending command lines, pushed by the worker thread, drained in frame() by
calling getCommandDispatcher().dispatchCommand(line.data(),
line.data()+line.size()) on each one. Same thread-safety guarantee, zero
file descriptors, zero pipe-buffer-full edge cases.
***********************************************************************/

/***********************************************************************
Per-instance argument parsing (-dir, -activation, -model, -device) --
v1 hand-rolls a strcasecmp loop over numArguments/arguments[] (the same
"textbook manual loop" pattern main.cpp uses in
../guide/style-example/style-example). Vrui now has a real declarative
option parser for exactly this: <Misc/CommandLineParser.h>,
Misc::CommandLineParser -- addValueOption()/addEnableOption()/
addCategoryOption() bind a variable to a flag with type conversion built
in, plus a --help screen generated for free. It's dated 2025-2026 in its
own header, so it may genuinely not have existed when v1 was written --
this isn't "v1 got it wrong," it's "there's now a nicer way." Optional:
the current loop is short enough (4 flags) that hand-rolling it is also
completely reasonable; worth using CommandLineParser if you want the
practice with a new Vrui class, not required for correctness.
***********************************************************************/

#ifndef VRUI_VISLETS_VOICEASSISTANT_INCLUDED
#define VRUI_VISLETS_VOICEASSISTANT_INCLUDED

#include <atomic>

#include <Geometry/Point.h>
#include <Geometry/Rotation.h>

#include <GL/GLColorTemplates.h>

#include <Misc/Autopointer.h>
#include <Misc/SelfDestructPointer.h>
#include <Misc/SizedTypes.h>

#include "SttEngine.h"

#include <Threads/Mutex.h>

#include <vector>

#include <Vrui/SoundContext.h>
#include <Vrui/Types.h>
#include <Vrui/Vislet.h>
#include <Vrui/UtilityTool.h>

/* Forward declarations: */

namespace Vrui
{

namespace Vislets
{

class VoiceAssistantFactory;

class VoiceAssistant : public Vislet
{
	friend class VoiceAssistantFactory;

	/* Embedded classes: */
	public:
	enum AssistantState
	{
		Warmup,
		Idle,
		Listening,
		Thinking,
		Speaking,
		Error
	};

	// Classes for activated voice assistant tool
	class VoiceAssistantToolFactory : public ToolFactory
	{
		friend class VoiceAssistantTool;
		friend class VoiceAssistant;

		/* Elements: */
		private:
		VoiceAssistant* voiceAssistant;

		/* Constructors and destructors: */
		public:
		VoiceAssistantToolFactory(ToolManager& toolManager, VoiceAssistant* sVoiceAssistant);
		virtual ~VoiceAssistantToolFactory(void);

		/* Methods from ToolFactory: */
		virtual const char* getName(void) const;
		virtual const char* getButtonFunction(int buttonSlotIndex) const;
		virtual Tool* createTool(const ToolInputAssignment& inputAssignment) const;
		virtual void destroyTool(Tool* tool) const;
	};

	class VoiceAssistantTool : public UtilityTool
	{
		friend class VoiceAssistantToolFactory;
		friend class VoiceAssistant;

		/* Elements: */
		private:
		static VoiceAssistantToolFactory* factory;

		/* Constructors and destructors: */
		public:
		VoiceAssistantTool(const ToolFactory* factory, const ToolInputAssignment& inputAssignment);

		/* Methods from Tool: */
		virtual const ToolFactory* getFactory(void) const;
		virtual void buttonCallback(int buttonSlotIndex, InputDevice::ButtonCallbackData* cbData);
	};

	/* Elements: */
	private:
	class HaloNode; // Scene graph node rendering the halo
	Misc::Autopointer<HaloNode> haloNode; // The halo node; added to the physical scene graph while the vislet is enabled
	Misc::SelfDestructPointer<VoiceAsst::SttEngine> sttEngine; // Speech-to-text engine, created in enable()

	static VoiceAssistantFactory* factory;
	static VoiceAssistant* owner; // The one and only VoiceAssistant instance, if any, that is currently enabled (the one that owns the tool)

	AssistantState state;
	double stateStartTime;
	bool userIsSpeaking;
	double userFinishedSpeakingTime;
	GLColor<GLfloat, 4> transitionStartColor;
	SoundContext* recordingSoundContext;
	Misc::Autopointer<SoundContext::RecordingCallback> micCallback;
	std::vector<Misc::SInt16> recordedSamples; // Mic samples captured during Listening, guarded by recordingMutex
	std::vector<Misc::SInt16> pendingSamples; // Finished recording waiting for speech recognition
	unsigned int recordingFrameRate;
	unsigned int recordingChannels;
	Threads::Mutex recordingMutex; // Guards recordedSamples between the audio thread and the main thread
	std::atomic<float> micLevel;
	float smoothedMicLevel;

	/* Private methods: */
	private:
	GLColor<GLfloat, 4> getStateColor(VoiceAssistant::AssistantState state) const;
	GLColor<GLfloat, 4> getHaloColor(double time) const; // Returns the current halo color, including any transition fade
	static void testCommandCallback(const char* argumentBegin, const char* argumentEnd, void* userData);
	static void voiceAssistantPressCallback(const char* argumentBegin, const char* argumentEnd, void* userData);
	static void voiceAssistantReleaseCallback(const char* argumentBegin, const char* argumentEnd, void* userData);
	void micCallbackMethod(const SoundContext::RecordingCallbackData& cbData);

	/* Constructors and destructors: */
	public:
	VoiceAssistant(int numArguments, const char* const arguments[]);
	virtual ~VoiceAssistant(void);

	/* Methods from Vislet: */
	virtual VisletFactory* getFactory(void) const;
	virtual void enable(bool startup);
	virtual void disable(bool shutdown);
	virtual void frame(void);

	/* Methods: */
	void applyState(AssistantState newState);
};

class VoiceAssistantFactory : public VisletFactory
{
	friend class VoiceAssistant;

	/* Elements: */
	private:
	std::string voskModelPath; // Folder of the Vosk speech recognition model

	int numHaloBands; // Gradient steps per edge; more = smoother (1..256)
	Scalar haloThickness; // Glow depth, as a fraction of the shorter visible screen side (0..0.5)
	Scalar haloMaxAlpha; // Glow brightness at the screen edge (0..1)
	Scalar haloFalloff; // How fast the glow fades inward; 1 = linear, higher = tighter rim (>0)
	Scalar haloPulseSpeed; // Breathing speed, in pulses per second; 0 = steady (>=0)
	Scalar haloMinPulse; // Lowest brightness while breathing or listening (0..1)
	Scalar haloMaxSizeListening; // Glow size at full voice; 1 = no growth (>=1)
	Scalar haloFalloffListening; // Fade at full voice; lower = glow spreads further inward (>0)
	double listeningBufferDuration; // Extra listening time after push-to-talk is released, in seconds (>0)
	double stateTransitionDuration; // color/fade time between states, in seconds; 0 = instant (>=0)
	Scalar micGain; // Mic loudness multiplier; higher = full glow at a quieter voice (>=0)
	double micLevelAttack; // How quickly the glow rises with your voice, in seconds (>0)
	double micLevelRelease; // How quickly the glow falls in pauses, in seconds (>0)
	GLColor<GLfloat, 4> warmupColor; // Glow color while warming up (alpha is forced to 1)
	GLColor<GLfloat, 4> listeningColor; // Glow color while listening (alpha is forced to 1)
	GLColor<GLfloat, 4> thinkingColor; // Glow color while thinking (alpha is forced to 1)
	GLColor<GLfloat, 4> speakingColor; // Glow color while speaking (alpha is forced to 1)
	GLColor<GLfloat, 4> errorColor; // Glow color on error (alpha is forced to 1)
	bool printStateChanges;

	/* Constructors and destructors: */
	public:
	VoiceAssistantFactory(VisletManager& visletManager);
	virtual ~VoiceAssistantFactory(void);

	/* Methods from VisletFactory: */
	virtual Vislet* createVislet(int numArguments, const char* const arguments[]) const;
	virtual void destroyVislet(Vislet* vislet) const;
};

}

}

#endif
