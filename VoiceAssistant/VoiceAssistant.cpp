/***********************************************************************
VoiceAssistant - See VoiceAssistant.h.
***********************************************************************/
// Open notes from Oliver:
// Audio: Vrui now contains centralized audio
// SoundContext class - 2 methods: registers a recording callback or removes it
// Auto starts and stops audio recording and playback
// Add or remove the callback to the sound context, and it will be called when audio is available
// Vosk chunk of audio straight from Vrui w translator function
// Config: the factory holds the defaults (done); per-instance overrides via the vislet's arguments are still to do

#include "VoiceAssistant.h"

#include <GL/gl.h>
#include <GL/GLColorTemplates.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <GL/GLValueCoders.h>

#include <Math/Constants.h>
#include <Math/Math.h>

#include <Misc/CommandDispatcher.h>
#include <Misc/ConfigurationFile.h>
#include <Misc/MessageLogger.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/SizedTypes.h>

#include <SceneGraph/GLRenderState.h>
#include <SceneGraph/GraphNode.h>

#include <string>

#include <Threads/FunctionCalls.h>

#include <utility>

#include <Vrui/DisplayState.h>
#include <Vrui/SceneGraphManager.h>
#include <Vrui/ToolManager.h>
#include <Vrui/VisletManager.h>
#include <Vrui/VRScreen.h>
#include <Vrui/Vrui.h>
#include <Vrui/VRWindow.h>

namespace Vrui
{

namespace Vislets
{

/*************************************
Used for the halo glow on the edges of the screen
*************************************/
typedef Geometry::Point<Scalar, 2> Point2; // A point on the screen plane, in screen coordinates
typedef Geometry::Vector<Scalar, 2> Vector2; // A direction on the screen plane

struct ScreenCorner
{
	Point2 position; // Position of the corner in screen coordinates
	Vector2 inward; // Direction from the corner toward the center of the screen, in screen coordinates

	ScreenCorner(const Point2& sPosition, const Vector2& sInward) :
		position(sPosition), inward(sInward)
	{
	}
};

/*************************************
Local functions of the c++ file
*************************************/

const char* getStateName(VoiceAssistant::AssistantState state)
{
	switch (state)
	{
		case VoiceAssistant::Warmup:
			return "Warmup";
		case VoiceAssistant::Idle:
			return "Idle";
		case VoiceAssistant::Listening:
			return "Listening";
		case VoiceAssistant::Thinking:
			return "Thinking";
		case VoiceAssistant::Speaking:
			return "Speaking";
		case VoiceAssistant::Error:
			return "Error";
	}
	return "Unknown";
}

GLColor<GLfloat, 4> VoiceAssistant::getStateColor(VoiceAssistant::AssistantState state) const
{
	switch (state)
	{
		case VoiceAssistant::Idle:
			return GLColor<GLfloat, 4>(0.0f, 0.0f, 0.0f, 0.0f); // Transparent
		case VoiceAssistant::Warmup:
			return factory->warmupColor; // Dim grey: powering up
		case VoiceAssistant::Listening:
			return factory->listeningColor; // Yellow: actively listening
		case VoiceAssistant::Thinking:
			return factory->thinkingColor; // Blue: processing
		case VoiceAssistant::Speaking:
			return factory->speakingColor; // Green: responding
		case VoiceAssistant::Error:
			return factory->errorColor; // Intense red: error
	}
	return GLColor<GLfloat, 4>(1.0f, 1.0f, 1.0f); // Unreachable if every case handled, keeps compiler happy
}

void drawEdgeGlow(const GLColor<GLfloat, 4>& color, const ScreenCorner& startCorner, const ScreenCorner& endCorner, const Scalar& haloWidth, const Scalar& haloFalloff, int numHaloBands, const Scalar& opacity)
{
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= numHaloBands; ++i)
	{
		Scalar inwardFraction = Scalar(i) / Scalar(numHaloBands); // How far into halo we are, 0=at edge, 1=at inner edge of halo
		Scalar inwardDistance = inwardFraction * haloWidth; // How far into the screen we are, in screen coordinates

		Scalar alpha = Math::pow(1.0 - inwardFraction, haloFalloff) * opacity; // How strong the glow is, 0=transparent, 1=full
		GLColor<GLfloat, 4> bandColor(color[0], color[1], color[2], GLfloat(alpha)); // Explicit narrowing: the alpha math is done in double, the color is float
		glColor(bandColor);

		glVertex(startCorner.position + startCorner.inward * inwardDistance);
		glVertex(endCorner.position + endCorner.inward * inwardDistance);
	}
	glEnd();
}

/*******************************************
Definition of class VoiceAssistant::HaloNode:
*******************************************/

class VoiceAssistant::HaloNode : public SceneGraph::GraphNode
{
	public:
	HaloNode(VoiceAssistant* sVoiceAssistant);
	virtual const char* getClassName(void) const;
	virtual void glRenderAction(SceneGraph::GLRenderState& renderState) const;

	private:
	VoiceAssistant* voiceAssistant;
};

/*************************************
Methods of class VoiceAssistant::HaloNode:
*************************************/
VoiceAssistant::HaloNode::HaloNode(VoiceAssistant* sVoiceAssistant) :
	voiceAssistant(sVoiceAssistant)
{
	/* Only traverse this node during the transparent OpenGL rendering pass: */
	passMask = SceneGraph::GraphNode::GLTransparentRenderPass;
}

const char* VoiceAssistant::HaloNode::getClassName(void) const
{
	return "VoiceAssistant::Halo";
}

void VoiceAssistant::HaloNode::glRenderAction(SceneGraph::GLRenderState& renderState) const
{
	const VoiceAssistantFactory* settings = voiceAssistant->factory;
	const double elapsed = getApplicationTime() - voiceAssistant->stateStartTime;
	// Don't draw anything in idle state
	if (voiceAssistant->state == VoiceAssistant::Idle && elapsed > settings->stateTransitionDuration) return;

	GLColor<GLfloat, 4> color = voiceAssistant->getHaloColor(getApplicationTime());

	// Get screen size and orientation
	const DisplayState& displayState = getDisplayState(renderState.contextData);
	const VRScreen* screen = displayState.screen;
	const Scalar screenWidth = screen->getWidth();
	const Scalar screenHeight = screen->getHeight();
	const VRWindow* window = displayState.window;
	const Scalar* panRect = window->getPanRect();

	const Scalar visibleLeft = panRect[0] * screenWidth;
	const Scalar visibleRight = panRect[1] * screenWidth;
	const Scalar visibleBottom = panRect[2] * screenHeight;
	const Scalar visibleTop = panRect[3] * screenHeight;

	Scalar haloWidth = Math::min(visibleRight - visibleLeft, visibleTop - visibleBottom) * voiceAssistant->factory->haloThickness;

	// Render state setup
	renderState.disableMaterials();
	renderState.disableTextures();
	renderState.disableCulling();
	renderState.blendFunc(GL_SRC_ALPHA, GL_ONE);
	renderState.uploadModelview();

	glPushMatrix();
	const ONTransform screenTransform = screen->getScreenTransformation();
	glMultMatrix(screenTransform);

	// Create 4 corner objects
	const ScreenCorner bottomLeft(Point2(visibleLeft, visibleBottom), Vector2(1, 1));
	const ScreenCorner bottomRight(Point2(visibleRight, visibleBottom), Vector2(-1, 1));
	const ScreenCorner topRight(Point2(visibleRight, visibleTop), Vector2(-1, -1));
	const ScreenCorner topLeft(Point2(visibleLeft, visibleTop), Vector2(1, -1));

	// Glow strength 0..1 (before max alpha and state fades) and size multiplier (1 = configured thickness)
	Scalar strength;
	Scalar sizeScale = 1.0;
	Scalar falloff = settings->haloFalloff;
	if (voiceAssistant->state == VoiceAssistant::Listening)
	{
		// Voice level 0..1: smoothed microphone loudness, amplified and capped
		const Scalar voiceLevel = Math::min(Scalar(voiceAssistant->smoothedMicLevel) * settings->micGain, Scalar(1));

		// Silence -> floor, full voice -> maximum, for both brightness and size
		strength = settings->haloMinPulse + (1 - settings->haloMinPulse) * voiceLevel;
		sizeScale = 1 + (settings->haloMaxSizeListening - 1) * voiceLevel;

		//change gradient slope based on voice level, so that the glow is more concentrated at the edges when the user is speaking
		falloff = settings->haloFalloff + (settings->haloFalloffListening - settings->haloFalloff) * voiceLevel;
	}
	else
	{
		// Idle breathing between the floor and full strength
		const Scalar wave = 0.5 + 0.5 * Math::sin(2.0 * Math::Constants<Scalar>::pi * elapsed * settings->haloPulseSpeed);
		strength = settings->haloMinPulse + (1 - settings->haloMinPulse) * wave;
	}

	// Grow with the voice, but never past half the visible area (opposite edges would overlap)
	const Scalar maxWidth = 0.5 * Math::min(visibleRight - visibleLeft, visibleTop - visibleBottom);
	haloWidth = Math::min(haloWidth * sizeScale, maxWidth);

	// State fades (color[3]) are applied last, so fading in and out of Idle still reaches 0
	const Scalar opacity = strength * settings->haloMaxAlpha * color[3];

	// Left edge
	drawEdgeGlow(color, bottomLeft, topLeft, haloWidth, falloff, settings->numHaloBands, opacity);

	// Right edge
	drawEdgeGlow(color, bottomRight, topRight, haloWidth, falloff, settings->numHaloBands, opacity);

	// Top edge
	drawEdgeGlow(color, topLeft, topRight, haloWidth, falloff, settings->numHaloBands, opacity);

	// Bottom edge
	drawEdgeGlow(color, bottomLeft, bottomRight, haloWidth, falloff, settings->numHaloBands, opacity);

	glPopMatrix();
}

/*******************************************
Methods of class VoiceAssistantToolFactory:
*******************************************/

VoiceAssistant::VoiceAssistantToolFactory::VoiceAssistantToolFactory(ToolManager& toolManager, VoiceAssistant* sVoiceAssistant) :
	ToolFactory("VoiceAssistantTool", toolManager),
	voiceAssistant(sVoiceAssistant)
{
	layout.setNumButtons(1);

	ToolFactory* utilityToolFactory = toolManager.loadClass("UtilityTool");
	utilityToolFactory->addChildClass(this);
	addParentClass(utilityToolFactory);

	VoiceAssistantTool::factory = this;
}

VoiceAssistant::VoiceAssistantToolFactory::~VoiceAssistantToolFactory(void)
{
	VoiceAssistantTool::factory = 0;
}

const char* VoiceAssistant::VoiceAssistantToolFactory::getName(void) const
{
	return "Voice Assistant Push-to-Talk";
}

const char* VoiceAssistant::VoiceAssistantToolFactory::getButtonFunction(int) const
{
	return "Push-to-Talk";
}

Tool* VoiceAssistant::VoiceAssistantToolFactory::createTool(const ToolInputAssignment& inputAssignment) const
{
	return new VoiceAssistantTool(this, inputAssignment);
}

void VoiceAssistant::VoiceAssistantToolFactory::destroyTool(Tool* tool) const
{
	delete tool;
}

/********************************************
Static elements of class VoiceAssistantTool:
********************************************/
VoiceAssistant::VoiceAssistantToolFactory* VoiceAssistant::VoiceAssistantTool::factory = 0;

/*********************************
Methods of class VoiceAssistantTool:
*********************************/

VoiceAssistant::VoiceAssistantTool::VoiceAssistantTool(const ToolFactory* factory, const ToolInputAssignment& inputAssignment) :
	UtilityTool(factory, inputAssignment)
{
}

const ToolFactory* VoiceAssistant::VoiceAssistantTool::getFactory(void) const
{
	return factory;
}

void VoiceAssistant::VoiceAssistantTool::buttonCallback(int buttonSlotIndex, InputDevice::ButtonCallbackData* cbData)
{
	if (cbData->newButtonState)
	{
		const char command[] = "voiceAssistant.press";
		// No need to include string.h because sizeof minus one is known at compile time
		Vrui::getCommandDispatcher().dispatchCommand(command, command + sizeof(command) - 1);
	}
	else
	{
		const char command[] = "voiceAssistant.release";
		Vrui::getCommandDispatcher().dispatchCommand(command, command + sizeof(command) - 1);
	}
}

/*************************************
Methods of class VoiceAssistant:
*************************************/

VoiceAssistant::VoiceAssistant(int numArguments, const char* const arguments[]) :
	Vislet(), haloNode(new HaloNode(this)), state(Idle), stateStartTime(0.0), userIsSpeaking(false), userFinishedSpeakingTime(0.0),
	transitionStartColor(GLColor<GLfloat, 4>(0.0f, 0.0f, 0.0f, 0.0f)), recordingSoundContext(nullptr),
	recordingFrameRate(0), recordingChannels(0),
	sttRunning(false), sttStartRequested(false), sttFinishRequested(false), transcriptReady(false),
	micLevel(0.0f), smoothedMicLevel(0.0f)
{
	if (owner == nullptr)
	{
		Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.test", &VoiceAssistant::testCommandCallback, this,
			"<stateName>", "Switches the assistant to the named state (Warmup|Idle|Listening|Thinking|Speaking|Error), for testing");

		// The 0s here are the argument descriptions: these commands take no arguments ('this' is the userData)
		Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.press", &VoiceAssistant::voiceAssistantPressCallback, this, 0,
			"Begin a voice assistant request (simulate pressing the button)");

		Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.release", &VoiceAssistant::voiceAssistantReleaseCallback, this, 0,
			"Release a voice assistant request (simulate releasing the button)");

		owner = this;
	}
	else
	{
		Misc::consoleWarning("VoiceAssistant: only one instance is allowed, ignoring this instance");
	}
}

VoiceAssistant::~VoiceAssistant(void)
{
	stopSttThread();

	Vrui::getCommandDispatcher().removeCommandCallback("voiceAssistant.test");
	Vrui::getCommandDispatcher().removeCommandCallback("voiceAssistant.press");
	Vrui::getCommandDispatcher().removeCommandCallback("voiceAssistant.release");

	if (owner == this)
	{
		owner = nullptr;
	}
}

VisletFactory* VoiceAssistant::getFactory(void) const
{
	return factory;
}

void VoiceAssistant::enable(bool startup)
{
	Vislet::enable(startup);
	Misc::consoleNote("VoiceAssistant: starting");

	if (owner != this) return;

	if (!sttEngine.isValid())
	{
		try
		{
			sttEngine = new VoiceAsst::SttEngine(factory->voskModelPath.c_str(), factory->sttVerbose);
			Misc::formattedConsoleNote("VoiceAssistant: loaded speech model %s", factory->voskModelPath.c_str());
		}
		catch (const std::runtime_error& err)
		{
			Misc::formattedConsoleError("VoiceAssistant: speech recognition unavailable: %s", err.what());
		}

		// Start the speech-to-text thread, if there is an engine for it to drive
		if (sttEngine.isValid())
		{
			{
				Threads::MutexCond::Lock lock(recordingCond);
				sttRunning = true;
			}
			sttThread.start(this, &VoiceAssistant::sttThreadMethod);
		}
	}

	// Add tool
	Vrui::getToolManager()->addClass(new VoiceAssistantToolFactory(*Vrui::getToolManager(), this),
		Vrui::ToolManager::defaultToolFactoryDestructor);

	// Start on warmup
	if (startup)
	{
		applyState(Warmup);
	}
	else
	{
		applyState(Idle);
	}

	// Add node to scene graph
	getSceneGraphManager()->addPhysicalNode(*haloNode);

	Misc::consoleNote("VoiceAssistant: started");
}

void VoiceAssistant::stopSttThread(void)
{
	if (!sttThread.isJoined())
	{
		{
			Threads::MutexCond::Lock lock(recordingCond);
			sttRunning = false;
			recordingCond.broadcast();
		}
		sttThread.join();
	}
}

void VoiceAssistant::disable(bool shutdown)
{
	Misc::consoleNote("VoiceAssistant: stopping");
	Vislet::disable(shutdown);

	if (owner != this) return;

	getSceneGraphManager()->removePhysicalNode(*haloNode);
	userIsSpeaking = false;
	applyState(Idle);

	stopSttThread();

	Vrui::getToolManager()->releaseClass(VoiceAssistantTool::factory);

	Misc::consoleNote("VoiceAssistant: stopped");
}

GLColor<GLfloat, 4> VoiceAssistant::getHaloColor(double time) const
{
	// Get the base color for the current state
	GLColor<GLfloat, 4> targetColor = getStateColor(state);

	GLColor<GLfloat, 4> oldColorCopy = transitionStartColor; // Make a copy of the transition start color to use for transition calculations
	GLColor<GLfloat, 4> targetColorCopy = targetColor; // Make a copy of the target color to use for transition calculations

	if (oldColorCopy[3] == 0.0f)
	{
		// If the transition start color is idle, set its color values to target
		oldColorCopy[0] = targetColorCopy[0];
		oldColorCopy[1] = targetColorCopy[1];
		oldColorCopy[2] = targetColorCopy[2];
	}

	else if (targetColorCopy[3] == 0.0f)
	{
		// If the target color is idle, set its color values to the transition start color
		targetColorCopy[0] = oldColorCopy[0];
		targetColorCopy[1] = oldColorCopy[1];
		targetColorCopy[2] = oldColorCopy[2];
	}

	// If we're in a transition, interpolate between the transition start color and the base color
	if (time - stateStartTime < factory->stateTransitionDuration)
	{
		double t = (time - stateStartTime) / factory->stateTransitionDuration;
		t = t * t * (3 - 2 * t); // Smoothstep interpolation

		targetColorCopy[0] = static_cast<GLfloat>(oldColorCopy[0] + t * (targetColorCopy[0] - oldColorCopy[0]));
		targetColorCopy[1] = static_cast<GLfloat>(oldColorCopy[1] + t * (targetColorCopy[1] - oldColorCopy[1]));
		targetColorCopy[2] = static_cast<GLfloat>(oldColorCopy[2] + t * (targetColorCopy[2] - oldColorCopy[2]));
		targetColorCopy[3] = static_cast<GLfloat>(oldColorCopy[3] + t * (targetColorCopy[3] - oldColorCopy[3]));
	}

	Scalar fade = 1.0;
	if (state == VoiceAssistant::Listening && !userIsSpeaking)
	{
		fade = Math::max(0.0, Math::min(1.0, 1 - (getApplicationTime() - userFinishedSpeakingTime) / factory->listeningBufferDuration));
	}
	targetColorCopy[3] *= static_cast<GLfloat>(fade);

	return targetColorCopy;
}

void VoiceAssistant::frame(void)
{
	double elapsed = getApplicationTime() - stateStartTime;

	// Pick up a finished transcript from sttThread, if there is one
	std::string heard;
	bool haveTranscript = false;
	{
		Threads::MutexCond::Lock lock(recordingCond);
		if (transcriptReady)
		{
			heard = transcript;
			transcriptReady = false;
			haveTranscript = true;
		}
	}
	if (haveTranscript && state == Thinking)
	{
		Misc::formattedConsoleNote("VoiceAssistant: heard \"%s\"", heard.c_str());
	}

	// FAKE PIPELINE FOR TESTING ONLY, until the real voice pipeline exists:
	// timers advance Warmup -> Idle, and after a press/release Listening -> Thinking -> Speaking -> Idle

	// Fake loading lasts 3 seconds for now
	const double warmupDuration = 3;
	if (state == Warmup && elapsed >= warmupDuration) applyState(Idle);

	const double thinkingDuration = 3; // Seconds before Thinking auto-advances to Speaking
	const double speakingDuration = 3; // Seconds before Speaking auto-advances to Idle

	const double errorDuration = 1; // Seconds before Error auto-advances to Idle

	// If not listening user not speaking, reset the userIsSpeaking flag
	if (state != Listening && userIsSpeaking)
	{
		userIsSpeaking = false;
	}

	// Update finished speaking timer if user not speaking
	if (state == Listening && !userIsSpeaking)
	{
		double elaspedSinceUserFinishedSpeaking = getApplicationTime() - userFinishedSpeakingTime;
		if (elaspedSinceUserFinishedSpeaking >= factory->listeningBufferDuration) applyState(Thinking);
	}
	else if (state == Thinking && elapsed >= thinkingDuration)
		applyState(Speaking);
	else if (state == Speaking && elapsed >= speakingDuration)
		applyState(Idle);
	else if (state == Error && elapsed >= errorDuration)
		applyState(Idle);
	// Update the microphone level for the halo glow, if in Listening state
	const float rawLevel = micLevel;

	float target = 0.0f;

	if (state == Listening)
	{
		target = rawLevel;
	}

	const float timeConstant = (target > smoothedMicLevel) ? factory->micLevelAttack : factory->micLevelRelease;

	const float amount = float(1.0f - Math::exp(-getFrameTime() / timeConstant));
	smoothedMicLevel += (target - smoothedMicLevel) * amount;

	// Update drawing (scheduled to next visual frame, can be configured)
	if (state != Idle || getApplicationTime() - stateStartTime < factory->stateTransitionDuration)
	{
		scheduleUpdate(getNextAnimationTime());
	}
}

void VoiceAssistant::applyState(AssistantState newState)
{
	if (state != newState)
	{
		// Leaving listening, disable mic
		if (state == Listening)
		{
			micLevel = 0.0f;
			smoothedMicLevel = 0.0f;
			if (recordingSoundContext != nullptr)
			{
				recordingSoundContext->removeRecordingCallback(*micCallback);
				recordingSoundContext = nullptr;

				// Tell sttThread the utterance is over: it feeds the last samples, then finishes
				{
					Threads::MutexCond::Lock lock(recordingCond);
					sttFinishRequested = true;
					recordingCond.signal();
				}
			}
		}

		// Entering listening, enable mic
		else if (newState == Listening)
		{
			recordingSoundContext = getRecordingSoundContext();
			if (recordingSoundContext == nullptr)
			{
				// No microphone available
				Misc::consoleWarning("VoiceAssistant: no microphone available");
				newState = Error;
			}
			else
			{
				const Sound::SoundDataFormat& format = recordingSoundContext->getRecordingFormat();
				if (format.bytesPerSample != sizeof(Misc::SInt16))
				{
					Misc::formattedConsoleWarning("VoiceAssistant: unsupported microphone format (%d bytes/sample, need 16-bit)",
						int(format.bytesPerSample));
					newState = Error;
				}
				else
				{
					recordingFrameRate = format.framesPerSecond;
					recordingChannels = format.samplesPerFrame;

					// Tell sttThread a new utterance is starting, with a clean buffer
					{
						Threads::MutexCond::Lock lock(recordingCond);
						recordedSamples.clear();
						sttStartRequested = true;
						sttFinishRequested = false;
						transcriptReady = false;
						recordingCond.signal();
					}

					micCallback = Threads::createFunctionCall(this, &VoiceAssistant::micCallbackMethod);
					recordingSoundContext->addRecordingCallback(*micCallback);
				}
			}
		}

		transitionStartColor = getHaloColor(getApplicationTime());
		state = newState;
		stateStartTime = getApplicationTime();
		// Use Vrui message logger to log state change without need for std::string
		if (factory->printStateChanges)
		{
			Misc::formattedConsoleNote("VoiceAssistant: state changed to %s", getStateName(state));
		}
	}
}

int no_mic = 0;

void* VoiceAssistant::sttThreadMethod(void)
{
	std::vector<Misc::SInt16> chunk; // Samples taken from recordedSamples, fed outside the lock

	while (true)
	{
		bool start = false;
		bool finish = false;
		{
			Threads::MutexCond::Lock lock(recordingCond);
			while (sttRunning && recordedSamples.empty() && !sttStartRequested && !sttFinishRequested)
				recordingCond.wait(lock);

			if (!sttRunning)
				break;

			start = sttStartRequested;
			sttStartRequested = false;
			std::swap(chunk, recordedSamples);
			finish = sttFinishRequested;
			sttFinishRequested = false;
		}

		// Slow work, outside the lock, in order: start, feed, finish
		if (start)
			sttEngine->begin(recordingFrameRate, recordingChannels);
		if (!chunk.empty())
		{
			sttEngine->feed(chunk.data(), chunk.size());
			chunk.clear();
		}
		if (finish)
		{
			std::string text = sttEngine->finish();
			Threads::MutexCond::Lock lock(recordingCond);
			transcript = text;
			transcriptReady = true;
		}
	}

	return 0;
}

void VoiceAssistant::micCallbackMethod(const SoundContext::RecordingCallbackData& cbData)
{
	const Misc::SInt16* samples = static_cast<const Misc::SInt16*>(cbData.frames);

	if (cbData.numFrames == 0)
	{
		return;
	}

	const size_t numSamples = cbData.numFrames * recordingChannels;

	// Store the recorded samples in a thread-safe manner to be sent to voice recognition
	{
		Threads::MutexCond::Lock recordingLock(recordingCond);
		recordedSamples.insert(recordedSamples.end(), samples, samples + numSamples);
		recordingCond.signal(); // Wake sttThread: there is audio to feed
	}

	double sumSquares = 0.0;
	for (size_t i = 0; i < numSamples; ++i)
	{
		double sample = samples[i];
		sumSquares += sample * sample;
	}

	double rms = Math::sqrt(sumSquares / numSamples) / 32768.0; // Normalize to [-1, 1] range for 16-bit signed integer

	// Atomic, read by frame() on main thread to set halo glow
	micLevel = float(rms);
}

// Console command to test state changes, for debugging or demonstration purposes
// Called in console via "voiceAssistant.test <stateName>"
// Syntax uses the state names defined in the AssistantState enum or letters
void VoiceAssistant::testCommandCallback(const char* argumentBegin, const char* argumentEnd, void* userData)
{
	VoiceAssistant* thisPtr = static_cast<VoiceAssistant*>(userData);
	if (!thisPtr->isActive())
	{
		Misc::consoleNote("VoiceAssistant: disabled, ignoring command");
		return;
	}
	std::string stateArg(argumentBegin, argumentEnd);

	AssistantState newState;
	if (strcasecmp(stateArg.c_str(), "warmup") == 0 || strcasecmp(stateArg.c_str(), "w") == 0) // Not case sensitive
		newState = Warmup;
	else if (strcasecmp(stateArg.c_str(), "idle") == 0 || strcasecmp(stateArg.c_str(), "i") == 0)
		newState = Idle;
	else if (strcasecmp(stateArg.c_str(), "listening") == 0 || strcasecmp(stateArg.c_str(), "l") == 0)
		newState = Listening;
	else if (strcasecmp(stateArg.c_str(), "thinking") == 0 || strcasecmp(stateArg.c_str(), "t") == 0)
		newState = Thinking;
	else if (strcasecmp(stateArg.c_str(), "speaking") == 0 || strcasecmp(stateArg.c_str(), "s") == 0)
		newState = Speaking;
	else if (strcasecmp(stateArg.c_str(), "error") == 0 || strcasecmp(stateArg.c_str(), "e") == 0)
		newState = Error;
	else
	{
		Misc::formattedConsoleWarning("VoiceAssistant: unknown state \"%s\"", stateArg.c_str());
		return;
	}

	thisPtr->applyState(newState);
}

// Called by the push-to-talk tool's button press, or by typing voiceAssistant.press in the console
void VoiceAssistant::voiceAssistantPressCallback(const char* argumentBegin, const char* argumentEnd, void* userData)
{
	VoiceAssistant* thisPtr = static_cast<VoiceAssistant*>(userData);
	if (!thisPtr->isActive())
	{
		Misc::consoleNote("VoiceAssistant: disabled, ignoring command");
		return;
	}
	// User can only talk when assistant is not busy (Idle, Speaking, or Error)
	// Allow user to interrupt speaking because the last command will have already been executed
	if (thisPtr->state != Idle && thisPtr->state != Speaking && thisPtr->state != Error)
	{
		Misc::formattedConsoleNote("VoiceAssistant: busy (%s), ignoring press", getStateName(thisPtr->state));
		return;
	}
	thisPtr->applyState(Listening);
	thisPtr->userIsSpeaking = true;
}

// Called by the push-to-talk tool's button release, or by typing voiceAssistant.release in the console
void VoiceAssistant::voiceAssistantReleaseCallback(const char* argumentBegin, const char* argumentEnd, void* userData)
{
	VoiceAssistant* thisPtr = static_cast<VoiceAssistant*>(userData);
	if (!thisPtr->isActive())
	{
		Misc::consoleNote("VoiceAssistant: disabled, ignoring command");
		return;
	}

	/* If it is still listening by time it is released,
	start timer before it actually stops listening to input */
	if (thisPtr->state == Listening)
	{
		thisPtr->userIsSpeaking = false;
		thisPtr->userFinishedSpeakingTime = getApplicationTime();
	}
}

/*************************************
Methods of class VoiceAssistantFactory:
*************************************/

VoiceAssistantFactory::VoiceAssistantFactory(VisletManager& visletManager) :
	VisletFactory("VoiceAssistant", visletManager),
	numHaloBands(24),
	haloThickness(0.08), haloMaxAlpha(0.8), haloFalloff(2.0), haloPulseSpeed(1.0),
	haloMinPulse(0.3), haloMaxSizeListening(1.8),
	haloFalloffListening(0.6),
	listeningBufferDuration(0.3), stateTransitionDuration(0.5),
	micGain(9.0), micLevelAttack(0.03), micLevelRelease(0.16),
	warmupColor(GLColor<GLfloat, 4>(1.0f, 0.55f, 0.1f)), // Orange: powering up
	listeningColor(GLColor<GLfloat, 4>(1.0f, 0.9f, 0.2f)), // Yellow: actively listening
	thinkingColor(GLColor<GLfloat, 4>(0.3f, 0.5f, 1.0f)), // Blue: processing
	speakingColor(GLColor<GLfloat, 4>(0.2f, 1.0f, 0.4f)), // Green: responding
	errorColor(GLColor<GLfloat, 4>(1.0f, 0.05f, 0.05f)), // Intense red: error
	printStateChanges(false),
	sttVerbose(false),
	ollamaUrl("http://127.0.0.1:11434"), ollamaModel("qwen3:4b"),
	ollamaKeepAlive(1800.0), ollamaTimeout(15.0),
	dryRun(false)
{
	// Set vosk model path
	const char* home = getenv("HOME");
	voskModelPath = std::string(home != nullptr ? home : "") + "/.local/share/vrui-assistant/models/vosk-model-en-us-0.22-lgraph";

	// Load settings from the vislet's section in Vrui's configuration; the defaults above stay for any missing tag
	Misc::ConfigurationFileSection cfs = visletManager.getVisletClassSection(getClassName());
	cfs.updateValue("./voskModelPath", voskModelPath);
	cfs.updateValue("./numHaloBands", numHaloBands);
	cfs.updateValue("./haloThickness", haloThickness);
	cfs.updateValue("./haloMaxAlpha", haloMaxAlpha);
	cfs.updateValue("./haloFalloff", haloFalloff);
	cfs.updateValue("./haloPulseSpeed", haloPulseSpeed);
	cfs.updateValue("./haloMinPulse", haloMinPulse);
	cfs.updateValue("./haloMaxSizeListening", haloMaxSizeListening);
	cfs.updateValue("./haloFalloffListening", haloFalloffListening);
	cfs.updateValue("./listeningBufferDuration", listeningBufferDuration);
	cfs.updateValue("./stateTransitionDuration", stateTransitionDuration);
	cfs.updateValue("./micGain", micGain);
	cfs.updateValue("./micLevelAttack", micLevelAttack);
	cfs.updateValue("./micLevelRelease", micLevelRelease);

	cfs.updateValue("./warmupColor", warmupColor);
	cfs.updateValue("./listeningColor", listeningColor);
	cfs.updateValue("./thinkingColor", thinkingColor);
	cfs.updateValue("./speakingColor", speakingColor);
	cfs.updateValue("./errorColor", errorColor);

	cfs.updateValue("./printStateChanges", printStateChanges);

	cfs.updateValue("./sttVerbose", sttVerbose);

	cfs.updateValue("./ollamaUrl", ollamaUrl);
	cfs.updateValue("./ollamaModel", ollamaModel);
	cfs.updateValue("./ollamaKeepAlive", ollamaKeepAlive);
	cfs.updateValue("./ollamaTimeout", ollamaTimeout);
	cfs.updateValue("./dryRun", dryRun);

	// Force alpha to 1 for all colors, so that the state fades are applied correctly in getHaloColor()
	warmupColor[3] = 1.0f;
	listeningColor[3] = 1.0f;
	thinkingColor[3] = 1.0f;
	speakingColor[3] = 1.0f;
	errorColor[3] = 1.0f;

	// Clamp every setting to its valid range, so a bad configuration value can't hang, NaN, or invert the halo
	numHaloBands = Math::clamp(numHaloBands, 1, 256);
	haloThickness = Math::clamp(haloThickness, Scalar(0), Scalar(0.5));
	haloMaxAlpha = Math::clamp(haloMaxAlpha, Scalar(0), Scalar(1));
	haloFalloff = Math::max(haloFalloff, Scalar(0.05));
	haloPulseSpeed = Math::max(haloPulseSpeed, Scalar(0));
	haloMinPulse = Math::clamp(haloMinPulse, Scalar(0), Scalar(1));
	haloMaxSizeListening = Math::max(haloMaxSizeListening, Scalar(1));
	haloFalloffListening = Math::max(haloFalloffListening, Scalar(0.05));
	listeningBufferDuration = Math::max(listeningBufferDuration, 0.01);
	stateTransitionDuration = Math::max(stateTransitionDuration, 0.0);
	micGain = Math::max(micGain, Scalar(0));
	micLevelAttack = Math::max(micLevelAttack, 0.001);
	micLevelRelease = Math::max(micLevelRelease, 0.001);
	ollamaKeepAlive = Math::max(ollamaKeepAlive, -1.0);
	ollamaTimeout = Math::clamp(ollamaTimeout, 1.0, 300.0);

	// Check that Ollama strings are plausible, warn when falling back so mistake isn't hidden
	if (ollamaUrl.compare(0, 7, "http://") != 0 && ollamaUrl.compare(0, 8, "https://") != 0)
	{
		Misc::formattedConsoleWarning("VoiceAssistant: ollamaUrl \"%s\" must start with http:// or https://; using the default", ollamaUrl.c_str());
		ollamaUrl = "http://127.0.0.1:11434";
	}
	if (ollamaModel.empty())
	{
		Misc::consoleWarning("VoiceAssistant: ollamaModel is empty; using qwen3:4b");
		ollamaModel = "qwen3:4b";
	}

	// Request sound processing, so Vrui starts its sound subsystem
	requestSound();

	// Publish the factory last, once all settings are loaded and valid
	VoiceAssistant::factory = this;
}

VoiceAssistantFactory::~VoiceAssistantFactory(void)
{
	VoiceAssistant::factory = nullptr;
}

Vislet* VoiceAssistantFactory::createVislet(int numArguments, const char* const arguments[]) const
{
	return new VoiceAssistant(numArguments, arguments);
}

void VoiceAssistantFactory::destroyVislet(Vislet* vislet) const
{
	delete vislet;
}

/*************************************
Boilerplate code for every Vrui plugin so the .so can be loaded at runtime:
*************************************/

extern "C" void resolveVoiceAssistantDependencies(Plugins::FactoryManager<VisletFactory>&)
{
}

extern "C" VisletFactory* createVoiceAssistantFactory(Plugins::FactoryManager<VisletFactory>& manager)
{
	VisletManager* visletManager = static_cast<VisletManager*>(&manager);
	return new VoiceAssistantFactory(*visletManager);
}

extern "C" void destroyVoiceAssistantFactory(VisletFactory* factory)
{
	delete factory;
}

/******************************************
Static elements of class VoiceAssistant:
******************************************/
VoiceAssistantFactory* VoiceAssistant::factory = nullptr;
VoiceAssistant* VoiceAssistant::owner = nullptr;
}

}
