/***********************************************************************
VoiceAssistant - See VoiceAssistant.h.
***********************************************************************/
//TRY RENDERING AS GLOW ON EDGES OF FRONT PLANE
// rectangle vignette style object
//USE SCENE GRAPH ARCHITECTURE, AND INSERT INTO SCENE GRAPH
// find rectange that fits the front planes of the viewer
// Put something in 3d space consistently for both eyes, and in front of the user, and at a consistent size
// -setConfig "Window/panningViewport=false"
// -vruiVerbose


// VRui now contains centralized audio
// SoundContext Class - 2 methods : registers recording callback or removes them
// auto starts and stops audio recording and playback
// add or remove the callback to the sound context, and it will be called when audio is available
// set default for how far into the screen the gradient goes
// all configurable
// look at some of vislets for how configuration works
// there is well configured space already in vrui on where vislet configs go
// vosk chunk of audio straight from vrui w translator function

// VISLETS HAVE ISACTIVE METHOD
// vislet config usuaslly associated with factory for vislet
// in vislet factory constructor - point me at config place and read the instructionf from thre
// look at device renderer vislet as example
// cave renderer (on vrui github) - has more config data
// factory has factory defualt config and the n the tool can override it

#include "VoiceAssistant.h"

#include <string>

#include <GL/gl.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLColorTemplates.h>

#include <Math/Math.h>

#include <Misc/CommandDispatcher.h>
#include <Misc/MessageLogger.h>

#include <SceneGraph/GraphNode.h>

#include <Vrui/SceneGraphManager.h>
#include <Vrui/ToolManager.h>
#include <Vrui/Vrui.h>
#include <Vrui/VisletManager.h>

namespace Vrui {

namespace Vislets {

/*******************************************
Definition of class VoiceAssistant::HaloNode:
*******************************************/

class VoiceAssistant::HaloNode:public SceneGraph::GraphNode
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
VoiceAssistant::HaloNode::HaloNode(VoiceAssistant* sVoiceAssistant)
	:voiceAssistant(sVoiceAssistant)
	{
	/* Only traverse this node during the transparent OpenGL rendering pass: */
	passMask=SceneGraph::GraphNode::GLTransparentRenderPass;
	}

const char* VoiceAssistant::HaloNode::getClassName(void) const
	{
	return "VoiceAssistant::Halo";
	}

void VoiceAssistant::HaloNode::glRenderAction(SceneGraph::GLRenderState& renderState) const
	{

	}

/*******************************************
Methods of class VoiceAssistantToolFactory:
*******************************************/

VoiceAssistant::VoiceAssistantToolFactory::VoiceAssistantToolFactory(ToolManager& toolManager,VoiceAssistant* sVoiceAssistant)
	:ToolFactory("VoiceAssistantTool",toolManager),
	 voiceAssistant(sVoiceAssistant)
	{
	layout.setNumButtons(1);

	ToolFactory* utilityToolFactory=toolManager.loadClass("UtilityTool");
	utilityToolFactory->addChildClass(this);
	addParentClass(utilityToolFactory);

	VoiceAssistantTool::factory=this;
	}

VoiceAssistant::VoiceAssistantToolFactory::~VoiceAssistantToolFactory(void)
	{
	VoiceAssistantTool::factory=0;
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
	return new VoiceAssistantTool(this,inputAssignment);
	}

void VoiceAssistant::VoiceAssistantToolFactory::destroyTool(Tool* tool) const
	{
	delete tool;
	}

/********************************************
Static elements of class VoiceAssistantTool:
********************************************/
VoiceAssistant::VoiceAssistantToolFactory* VoiceAssistant::VoiceAssistantTool::factory=0;

/*********************************
Methods of class VoiceAssistantTool:
*********************************/

VoiceAssistant::VoiceAssistantTool::VoiceAssistantTool(const ToolFactory* factory,const ToolInputAssignment& inputAssignment)
	:UtilityTool(factory,inputAssignment)
	{
	}

const ToolFactory* VoiceAssistant::VoiceAssistantTool::getFactory(void) const
	{
	return factory;
	}

void VoiceAssistant::VoiceAssistantTool::buttonCallback(int buttonSlotIndex,InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState)
		{
		const char command[] = "voiceAssistant.press";
    	// No need to include string.h because sizeof minus one is known at compile time
		Vrui::getCommandDispatcher().dispatchCommand(command,command+sizeof(command)-1);
		}
	else
		{
		const char command[] = "voiceAssistant.release";
		Vrui::getCommandDispatcher().dispatchCommand(command,command+sizeof(command)-1);
		}
	}

/*************************************
Methods of class VoiceAssistant:
*************************************/
VoiceAssistant::VoiceAssistant(int numArguments,const char* const arguments[])
	:Vislet(), haloNode(new HaloNode(this)),state(Idle),stateStartTime(0.0),userIsSpeaking(false),userFinishedSpeakingTime(0.0)
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
	//Implement to actually use the startup command line arg
	//Startup is only true on the first enable call
	//Remove the command Callbacks when vislet disabled

	// SOLUTION - if enabled or disabled in the callbacks, then have them just print to the console that it is inactive

	Vislet::enable(startup);
	Misc::consoleNote("VoiceAssistant: starting");

	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.test",&VoiceAssistant::testCommandCallback,this,
		"<stateName>","Switches the orb to the named state (Warmup|Idle|Listening|Thinking|Speaking|Error), for testing");

	// 0s here serve as null pointer for userData, since the callbacks don't need it
	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.press",&VoiceAssistant::voiceAssistantPressCallback,this,0,
		"Begin a voice assistant request (simulate pressing the button)");

	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.release",&VoiceAssistant::voiceAssistantReleaseCallback,this,0,
		"Release a voice assistant request (simulate releasing the button)");

	//Add tool
	Vrui::getToolManager()->addClass(new VoiceAssistantToolFactory(*Vrui::getToolManager(),this),
		Vrui::ToolManager::defaultToolFactoryDestructor);

	//start on warmup
	applyState(Warmup);

	//add node to scene graph
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
	double elapsed = getApplicationTime()-stateStartTime;

	// THESE ARE FOR TESTING PURPOSES ONLY, to demonstrate state changes without needing a voice input
	// Each press command will cycle through these states

	// Fake loading lasts 3 seconds for now
	const double warmupDuration = 3;
	if(state==Warmup && elapsed>=warmupDuration) applyState(Idle);

	//Give user a bit more time to talk after lifting the button
	const double listeningBufferDuration = .3;
	const double thinkingDuration = 3; // seconds before Thinking auto-advances to Speaking
	const double speakingDuration = 3; // seconds before Speaking auto-advances to Idle

	// If not listening user not speaking, reset the userIsSpeaking flag
	if(state!=Listening && userIsSpeaking)
		{
		userIsSpeaking = false;
		}

	// Update finished speaking timer if user not speaking
	if(state==Listening && !userIsSpeaking)
		{
			double elaspedSinceUserFinishedSpeaking = getApplicationTime()-userFinishedSpeakingTime;
			if(elaspedSinceUserFinishedSpeaking>=listeningBufferDuration) applyState(Thinking);
		}
	else if(state==Thinking && elapsed>=thinkingDuration) applyState(Speaking);
	else if(state==Speaking && elapsed>=speakingDuration) applyState(Idle);

	// Update drawing (scheduled to next visual frame, can be configured)
	if(state!=Idle)
		scheduleUpdate(getNextAnimationTime());
	}

// Local function to this cpp file for debugging or informational purposes:
// returns a string representation of the AssistantState enum

const char* getStateName(VoiceAssistant::AssistantState state)
	{
	switch(state)
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

// Local function to this cpp file
GLColor<GLfloat,4> getStateColor(VoiceAssistant::AssistantState state)
	{
	switch(state)
		{
		case VoiceAssistant::Warmup:
			return GLColor<GLfloat,4>(0.9f,0.9f,0.95f); // dim grey: powering up
		case VoiceAssistant::Idle:
			return GLColor<GLfloat,4>(0.3f,0.5f,0.6f); // muted cyan-grey: resting
		case VoiceAssistant::Listening:
			return GLColor<GLfloat,4>(1.0f,0.9f,0.2f); // yellow: actively listening
		case VoiceAssistant::Thinking:
			return GLColor<GLfloat,4>(0.3f,0.5f,1.0f); // blue: processing
		case VoiceAssistant::Speaking:
			return GLColor<GLfloat,4>(0.2f,1.0f,0.4f); // green: responding
		case VoiceAssistant::Error:
			return GLColor<GLfloat,4>(1.0f,0.05f,0.05f); // intense red: error
		}
	return GLColor<GLfloat,4>(1.0f,1.0f,1.0f); // unreachable if every case handled, keeps compiler happy
	}

void VoiceAssistant::applyState(AssistantState newState)
	{
	if(state != newState)
		{
		state = newState;
		stateStartTime = getApplicationTime();

		// Use Vrui message logger to log state change without need for std::string
		Misc::formattedConsoleNote("VoiceAssistant: state changed to %s",getStateName(state));
		}
	}

// Console command to test state changes, for debugging or demonstration purposes
// Called in console via "voiceAssistant.test <stateName>"
// Syntax uses the state names defined in the OrbState enum or letters
void VoiceAssistant::testCommandCallback(const char* argumentBegin,const char* argumentEnd,void* userData)
	{
	VoiceAssistant* thisPtr=static_cast<VoiceAssistant*>(userData);
	std::string stateArg(argumentBegin,argumentEnd);

	AssistantState newState;
	if(strcasecmp(stateArg.c_str(),"warmup")==0 || strcasecmp(stateArg.c_str(),"w")==0) // Not case sensitive
		newState=Warmup;
	else if(strcasecmp(stateArg.c_str(),"idle")==0 || strcasecmp(stateArg.c_str(),"i")==0)
		newState=Idle;
	else if(strcasecmp(stateArg.c_str(),"listening")==0 || strcasecmp(stateArg.c_str(),"l")==0)
		newState=Listening;
	else if(strcasecmp(stateArg.c_str(),"thinking")==0 || strcasecmp(stateArg.c_str(),"t")==0)
		newState=Thinking;
	else if(strcasecmp(stateArg.c_str(),"speaking")==0 || strcasecmp(stateArg.c_str(),"s")==0)
		newState=Speaking;
	else if(strcasecmp(stateArg.c_str(),"error")==0 || strcasecmp(stateArg.c_str(),"e")==0)
		newState=Error;
	else
		{
		Misc::formattedConsoleWarning("VoiceAssistant: unknown state \"%s\"",stateArg.c_str());
		return;
		}

	thisPtr->applyState(newState);
	}

//Called by saying voiceAssistant.press
void VoiceAssistant::voiceAssistantPressCallback(const char* argumentBegin,const char* argumentEnd,void* userData)
	{
	VoiceAssistant* thisPtr=static_cast<VoiceAssistant*>(userData);
	thisPtr->applyState(Listening);
	thisPtr->userIsSpeaking = true;
	}

//Called by saying voiceAssistant.release
void VoiceAssistant::voiceAssistantReleaseCallback(const char* argumentBegin,const char* argumentEnd,void* userData)
	{
	VoiceAssistant* thisPtr=static_cast<VoiceAssistant*>(userData);

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

VoiceAssistantFactory::VoiceAssistantFactory(VisletManager& visletManager)
	:VisletFactory("VoiceAssistant",visletManager)
	{
	VoiceAssistant::factory=this;
	}

VoiceAssistantFactory::~VoiceAssistantFactory(void)
	{
	VoiceAssistant::factory=0;
	}

Vislet* VoiceAssistantFactory::createVislet(int numArguments,const char* const arguments[]) const
	{
	return new VoiceAssistant(numArguments,arguments);
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
	VisletManager* visletManager=static_cast<VisletManager*>(&manager);
	return new VoiceAssistantFactory(*visletManager);
	}

extern "C" void destroyVoiceAssistantFactory(VisletFactory* factory)
	{
	delete factory;
	}

/******************************************
Static elements of class VoiceAssistant:
******************************************/
VoiceAssistantFactory* VoiceAssistant::factory=0;

}

}
