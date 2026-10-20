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

#include <string>

#include <GL/gl.h>
#include <GL/GLColorTemplates.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>

#include <Math/Math.h>
#include <Math/Constants.h>

#include <Misc/CommandDispatcher.h>
#include <Misc/ConfigurationFile.h>
#include <Misc/MessageLogger.h>
#include <Misc/StandardValueCoders.h>

#include <SceneGraph/GLRenderState.h>
#include <SceneGraph/GraphNode.h>

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

GLColor<GLfloat, 4> getStateColor(VoiceAssistant::AssistantState state)
{
	switch (state)
	{
		case VoiceAssistant::Warmup:
			return GLColor<GLfloat, 4>(0.9f, 0.9f, 0.95f); // Dim grey: powering up
		case VoiceAssistant::Idle:
			return GLColor<GLfloat, 4>(0.3f, 0.5f, 0.6f); // Muted cyan-grey: resting
		case VoiceAssistant::Listening:
			return GLColor<GLfloat, 4>(1.0f, 0.9f, 0.2f); // Yellow: actively listening
		case VoiceAssistant::Thinking:
			return GLColor<GLfloat, 4>(0.3f, 0.5f, 1.0f); // Blue: processing
		case VoiceAssistant::Speaking:
			return GLColor<GLfloat, 4>(0.2f, 1.0f, 0.4f); // Green: responding
		case VoiceAssistant::Error:
			return GLColor<GLfloat, 4>(1.0f, 0.05f, 0.05f); // Intense red: error
	}
	return GLColor<GLfloat, 4>(1.0f, 1.0f, 1.0f); // Unreachable if every case handled, keeps compiler happy
}

void drawEdgeGlow(const GLColor<GLfloat, 4>& color, const ScreenCorner& startCorner, const ScreenCorner& endCorner, const Scalar& haloWidth, const Scalar& haloFalloff, const int numBands, const Scalar& opacity)
{
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= numBands; ++i)
	{
		Scalar inwardFraction = Scalar(i) / Scalar(numBands); // How far into halo we are, 0=at edge, 1=at inner edge of halo
		Scalar inwardDistance = inwardFraction * haloWidth; // How far into the screen we are, in screen coordinates

		Scalar alpha = Math::pow(1.0 - inwardFraction, haloFalloff) * opacity; // How strong the glow is, 0=transparent, 1=full
		GLColor<GLfloat, 4> bandColor(color[0], color[1], color[2], GLfloat(alpha)); // Explicit narrowing: the alpha math is done in double, the colour is float
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
	// Don't draw anything in idle state
	if (voiceAssistant->state == VoiceAssistant::Idle) return;

	// Number of bands in custom gradient, more bands = smoother gradient, but more expensive to render
	const int numBands = 24;
	const Scalar minPulse = 0.4; // How dim the glow gets at its minimum, 0=off, 1=always full brightness

	const double elapsed = getApplicationTime() - voiceAssistant->stateStartTime;
	const Scalar wave = 0.5 + 0.5 * Math::sin(2.0 * Math::Constants<Scalar>::pi * elapsed * voiceAssistant->factory->haloPulseSpeed); // Oscillates between 0 and 1, one full cycle every 1/haloPulseSpeed seconds
	const Scalar pulse = minPulse + (1 - minPulse) * wave;
	const Scalar opacity = pulse * voiceAssistant->factory->haloMaxAlpha;

	GLColor<GLfloat, 4> color = getStateColor(voiceAssistant->state);

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

	const Scalar haloWidth = Math::min(visibleRight - visibleLeft, visibleTop - visibleBottom) * voiceAssistant->factory->haloThickness;

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

	// Left edge
	drawEdgeGlow(color, bottomLeft, topLeft, haloWidth, voiceAssistant->factory->haloFalloff, numBands, opacity);

	// Right edge
	drawEdgeGlow(color, bottomRight, topRight, haloWidth, voiceAssistant->factory->haloFalloff, numBands, opacity);

	// Top edge
	drawEdgeGlow(color, topLeft, topRight, haloWidth, voiceAssistant->factory->haloFalloff, numBands, opacity);

	// Bottom edge
	drawEdgeGlow(color, bottomLeft, bottomRight, haloWidth, voiceAssistant->factory->haloFalloff, numBands, opacity);

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
	Vislet(), haloNode(new HaloNode(this)), state(Idle), stateStartTime(0.0), userIsSpeaking(false), userFinishedSpeakingTime(0.0)
{
}

VoiceAssistant::~VoiceAssistant(void)
{
}

VisletFactory* VoiceAssistant::getFactory(void) const
{
	return factory;
}

void VoiceAssistant::enable(bool startup)
{
	// TODO: use the startup argument (it is only true on the first enable call)
	// TODO: the command callbacks are registered on every enable and never removed on disable.
	// Option: keep them registered, and have them check isActive() and print that the assistant is inactive

	Vislet::enable(startup);
	Misc::consoleNote("VoiceAssistant: starting");

	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.test", &VoiceAssistant::testCommandCallback, this,
		"<stateName>", "Switches the assistant to the named state (Warmup|Idle|Listening|Thinking|Speaking|Error), for testing");

	// The 0s here are the argument descriptions: these commands take no arguments ('this' is the userData)
	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.press", &VoiceAssistant::voiceAssistantPressCallback, this, 0,
		"Begin a voice assistant request (simulate pressing the button)");

	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.release", &VoiceAssistant::voiceAssistantReleaseCallback, this, 0,
		"Release a voice assistant request (simulate releasing the button)");

	// Add tool
	Vrui::getToolManager()->addClass(new VoiceAssistantToolFactory(*Vrui::getToolManager(), this),
		Vrui::ToolManager::defaultToolFactoryDestructor);

	// Start on warmup
	applyState(Warmup);

	// Add node to scene graph
	getSceneGraphManager()->addPhysicalNode(*haloNode);
}

void VoiceAssistant::disable(bool shutdown)
{
	Vrui::getToolManager()->releaseClass(VoiceAssistantTool::factory);
	Vislet::disable(shutdown);
	Misc::consoleNote("VoiceAssistant: stopping");

	// Remove node from scene graph
	getSceneGraphManager()->removePhysicalNode(*haloNode);
}

void VoiceAssistant::frame(void)
{
	double elapsed = getApplicationTime() - stateStartTime;

	// FAKE PIPELINE FOR TESTING ONLY, until the real voice pipeline exists:
	// timers advance Warmup -> Idle, and after a press/release Listening -> Thinking -> Speaking -> Idle

	// Fake loading lasts 3 seconds for now
	const double warmupDuration = 3;
	if (state == Warmup && elapsed >= warmupDuration) applyState(Idle);

	// Give user a bit more time to talk after lifting the button
	const double listeningBufferDuration = .3;
	const double thinkingDuration = 3; // Seconds before Thinking auto-advances to Speaking
	const double speakingDuration = 3; // Seconds before Speaking auto-advances to Idle

	// If not listening user not speaking, reset the userIsSpeaking flag
	if (state != Listening && userIsSpeaking)
	{
		userIsSpeaking = false;
	}

	// Update finished speaking timer if user not speaking
	if (state == Listening && !userIsSpeaking)
	{
		double elaspedSinceUserFinishedSpeaking = getApplicationTime() - userFinishedSpeakingTime;
		if (elaspedSinceUserFinishedSpeaking >= listeningBufferDuration) applyState(Thinking);
	}
	else if (state == Thinking && elapsed >= thinkingDuration)
		applyState(Speaking);
	else if (state == Speaking && elapsed >= speakingDuration)
		applyState(Idle);

	// Update drawing (scheduled to next visual frame, can be configured)
	if (state != Idle)
		scheduleUpdate(getNextAnimationTime());
}

void VoiceAssistant::applyState(AssistantState newState)
{
	if (state != newState)
	{
		state = newState;
		stateStartTime = getApplicationTime();

		// Use Vrui message logger to log state change without need for std::string
		Misc::formattedConsoleNote("VoiceAssistant: state changed to %s", getStateName(state));
	}
}

// Console command to test state changes, for debugging or demonstration purposes
// Called in console via "voiceAssistant.test <stateName>"
// Syntax uses the state names defined in the AssistantState enum or letters
void VoiceAssistant::testCommandCallback(const char* argumentBegin, const char* argumentEnd, void* userData)
{
	VoiceAssistant* thisPtr = static_cast<VoiceAssistant*>(userData);
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
	thisPtr->applyState(Listening);
	thisPtr->userIsSpeaking = true;
}

// Called by the push-to-talk tool's button release, or by typing voiceAssistant.release in the console
void VoiceAssistant::voiceAssistantReleaseCallback(const char* argumentBegin, const char* argumentEnd, void* userData)
{
	VoiceAssistant* thisPtr = static_cast<VoiceAssistant*>(userData);

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
	haloThickness(0.08), haloMaxAlpha(0.8), haloFalloff(2.0), haloPulseSpeed(1.0)
{
	VoiceAssistant::factory = this;

	Misc::ConfigurationFileSection cfs = visletManager.getVisletClassSection(getClassName());
	cfs.updateValue("./haloThickness", haloThickness);
	cfs.updateValue("./haloMaxAlpha", haloMaxAlpha);
	cfs.updateValue("./haloFalloff", haloFalloff);
	cfs.updateValue("./haloPulseSpeed", haloPulseSpeed);
}

VoiceAssistantFactory::~VoiceAssistantFactory(void)
{
	VoiceAssistant::factory = 0;
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
VoiceAssistantFactory* VoiceAssistant::factory = 0;

}

}
