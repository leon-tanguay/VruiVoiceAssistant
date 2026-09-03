/***********************************************************************
VoiceAssistant - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md
section 2, and ../guide/RECODE_CHECKLIST.md item 11 for what this file is
supposed to do and what's wrong with v1's version -- most notably the
duplicated orb-state fields. CORRECTED: the OpenAL earcon path is NOT
dead code to delete -- see the audio comment below and
../guide/DEPENDENCIES.md #11, which has been rewritten).
***********************************************************************/

/***********************************************************************
DRAWING THE ORB -- reference implementation named directly: Vrui's own
TurnSignal vislet (~/src/vrui/Vrui/Vislets/TurnSignal.{h,cpp}). It's a
real, shipped example of exactly this problem -- a small head-locked
indicator giving the user feedback (which way to turn) -- and it settles
several questions an earlier draft of this comment had to guess at.

The rule, stated plainly: never render in 2D or in screen space. It does
not work in VR -- a flat pixel overlay is pinned to ONE window's
rectangle, meaningless on an HMD (there is no "the screen"), and renders
wrong in stereo (both eyes would show the identical flat image with no
disparity). TurnSignal instead draws its arrow as a real, flat 3D object
positioned and oriented relative to the user's head, using ordinary
immediate-mode OpenGL -- no special "HUD" API, no glOrtho.

The exact recipe, read from TurnSignal.cpp's display() (lines 251-317):

    Point viewerPos=getMainViewer()->getHeadPosition();
    Vector viewDir=getMainViewer()->getViewDirection();
    glPushMatrix();
    glTranslate(viewerPos-Point::origin);
    glRotate(Rotation::fromBaseVectors(viewDir^getUpDirection(),getUpDirection()));
    glTranslate(Scalar(0),arrowHeight,-arrowDist);   // up, then forward (OpenGL -Z)
    glScale(arrowSize,arrowSize,arrowSize);
    // ...draw with plain glBegin/glVertex/glColor, in this local frame...
    glPopMatrix();

Notably: **no `goToPhysicalSpace()` call.** TurnSignal's own comment labels the state
right after the final `glPopMatrix()` as "physical coordinates" -- meaning a
Vislet's `display()` is apparently already handed physical-space coordinates
by Vrui's render loop (unlike an Application's own scene rendering, which is
navigational and would need `goToPhysicalSpace(contextData)` to escape it).
Confirmed by reading TurnSignal's actual, shipping code; not independently
verified against Vrui's own render-loop source, so if the orb's geometry
comes out double-transformed or misplaced, that assumption is the first
thing to check.

Piece by piece:
- `getMainViewer()->getHeadPosition()`/`getViewDirection()`/`getUpDirection()`
  (<Vrui/Viewer.h>) -- all physical-space, exactly as before.
- `Rotation::fromBaseVectors(xAxis,yAxis)` (<Geometry/Rotation.h>) builds an
  orthonormal rotation from two basis vectors -- here, "right" (view direction
  crossed with up, `^` is Vrui's cross-product operator on Vector) as the local
  X axis and "up" as the local Y axis. This is the toolkit's ready-made answer
  to "build a rotation that faces this direction with this up vector" --
  don't hand-derive a rotation matrix.
  - CORRECTED (an earlier draft of this note guessed wrong): SAME order as
    TurnSignal, not the other way around. Worked out by hand from
    fromBaseVectors(viewDir^up,up) with viewDir=(0,0,-1),up=(0,1,0): local X
    (xAxis) comes out (1,0,0), and local Z -- implied by the right-handed
    basis, xAxis-cross-yAxis -- comes out (0,0,1), which is BEHIND the
    viewer's view direction, i.e. back toward the viewer. TurnSignal's flat
    arrow is drawn in the local XY plane (plain 2-argument glVertex(x,y) calls,
    z implicitly 0), so its face normal already points at the viewer with this
    exact call, unchanged. It only reads as "facing away" if you assume the
    arrow lies flat on the ground -- it doesn't; it's translated up and
    forward (0,arrowHeight,-arrowDist), a vertical card in front of you, not
    a ground decal. Practical result: local +X is screen-right and local +Y
    is screen-up from the viewer's standpoint, same as drawing on ordinary 2D
    graph paper -- no mirroring to work out, no flip needed.
- `arrowSize`/`arrowDist`/`arrowHeight` are real physical-unit constants
  (TurnSignal.h line 46-48, defaulted via `Vrui::getInchFactor()` --
  <Vrui/Vrui.h>, "length of an inch in Vrui physical units" -- and loaded
  from a `Misc::ConfigurationFileSection` exactly like the settings pattern
  already documented elsewhere in this file). The orb's size/distance/height
  should be the same kind of named, configurable, physical-unit constants,
  not a magic number buried in display().
- The `(0,arrowHeight,-arrowDist)` translate happens AFTER the rotation, so
  it moves along the NEW (rotated) axes -- forward is -Z in OpenGL's
  convention, which is why the distance is negated.

Ask your boss to confirm this matches their own convention for anything not
explicitly nailed down above (how far in front of the head, whether the two
orbs' corner-like arrangement should be two separate head-relative points or
one anchor point with a local offset) -- TurnSignal answers "how do I place
one flat thing relative to the head," not "how do I place two."

CORNER-HUD VARIANT (current direction, per direct instruction): same recipe
as above, unchanged -- head-locked, recomputed every frame, physical-space,
no glOrtho -- just pushed toward the corner of view instead of dead-center.
Concretely: after the same glTranslate(headPos)/glRotate(fromBaseVectors(
right,up)), use a LARGER rightOffset and upOffset (both positive: local +X is
screen-right, local +Y is screen-up, per the derivation above) relative to
forwardDist, e.g. glTranslate(rightOffset,upOffset,-forwardDist), so the shape
sits toward the top-right of the field of view rather than centered in front
of the face like TurnSignal's arrow. Also make it visually flat/2D-styled
(unlit glColor + glBegin/glVertex in the local XY plane, z implicit 0 -- same
technique as TurnSignal's arrow) instead of the lit glDrawSphereIcosahedron
ball -- a HUD icon reads better flat and unlit than as a shaded 3D sphere.
Since this must track the CURRENT head pose every frame (that's what makes it
read as "pinned to the corner of my view" as you look around), the position/
orientation can no longer be computed once in enable() and held fixed --
recompute every frame, same as TurnSignal does directly inside display().

VISUAL STYLE: design the orb after Destiny's Ghost -- see
../guide/BUILD_ORDER.md Milestone 1 for the full direction (angular shell
plates instead of a plain ring, a single glowing "eye" carrying most of the
expressiveness, matte-grey-plus-cyan palette vs. today's per-state colour
scheme -- an open decision to make before writing drawOrb()'s colour logic,
not after). Doesn't change anything about the physical-space placement
above, only the shapes/colours drawn once placed.
***********************************************************************/

/***********************************************************************
AUDIO -- sound should come from the orb's location, not play flat.
Direct guidance: "sound output from an assistant should come from the 3D
location of the assistant's graphical representation." This REVERSES an
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

#include <Vrui/Vislet.h>
#include <Vrui/Types.h>
#include <Geometry/Point.h>
#include <Geometry/Rotation.h>

class GLContextData;

namespace Vrui {

namespace Vislets {

class VoiceAssistantFactory;

class VoiceAssistant:public Vislet
	{
	friend class VoiceAssistantFactory;

	/* Embedded classes: */
	public:
	enum OrbState { Warming, Idle, Listening, Thinking, Speaking, Error };

	/* Elements: */
	private:
	static VoiceAssistantFactory* factory;
	Point orbPosition;
	Rotation orbOrientation;
	OrbState state;
	double stateStartTime;

	/* Constructors and destructors: */
	public:
	VoiceAssistant(int numArguments,const char* const arguments[]);
	virtual ~VoiceAssistant(void);

	/* Methods from Vislet: */
	virtual VisletFactory* getFactory(void) const;
	virtual void enable(bool startup);
	virtual void disable(bool shutdown);
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;

	/* Methods: */
	void applyState(OrbState newState);
	};

class VoiceAssistantFactory:public VisletFactory
	{
	friend class VoiceAssistant;

  	/* Constructors and destructors: */
  	public:
 	VoiceAssistantFactory(VisletManager& visletManager);
  	virtual ~VoiceAssistantFactory(void);

  	/* Methods from VisletFactory: */
  	virtual Vislet* createVislet(int numArguments,const char* const arguments[]) const;
  	virtual void destroyVislet(Vislet* vislet) const;
  	};

}

}

#endif
