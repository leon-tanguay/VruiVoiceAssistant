/***********************************************************************
VoiceAssistant - See VoiceAssistant.h.
***********************************************************************/

#include "VoiceAssistant.h"

#include <string>

#include <GL/gl.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLColorTemplates.h>

#include <Math/Math.h>

#include <Misc/CommandDispatcher.h>
#include <Misc/MessageLogger.h>

#include <Vrui/Vrui.h>
#include <Vrui/Viewer.h>
#include <Vrui/VisletManager.h>

namespace Vrui {

namespace Vislets {

/*************************************
Methods of class VoiceAssistant:
*************************************/
VoiceAssistant::VoiceAssistant(int numArguments,const char* const arguments[])
	:Vislet(), state(Idle), stateStartTime(0.0)
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
	Vislet::enable(startup);
	Misc::consoleNote("VoiceAssistant: starting");

	Vrui::getCommandDispatcher().addCommandCallback("voiceAssistant.test",&VoiceAssistant::testCommandCallback,this,
		"<stateName>","Switches the orb to the named state (Warmup|Idle|Listening|Thinking|Speaking|Error), for testing");
	}

void VoiceAssistant::disable(bool shutdown)
	{
	Vislet::disable(shutdown);
	Misc::consoleNote("VoiceAssistant: stopping");
	}
	
void VoiceAssistant::frame(void)
	{
	// Update the assistant's position and orientation based on main viewer's position
	Point headPos = getMainViewer()->getHeadPosition();
	Vector viewDir = getMainViewer()->getViewDirection();
	Vector up = getUpDirection();
	Vector right = viewDir ^ up; // cross product to get right vector

	Scalar forwardDist = Scalar(1.5)*getInchFactor();  // how far in front
	Scalar rightOffset = Scalar(.25)*getInchFactor();   // pushed toward the right edge
	Scalar upOffset    = Scalar(.25)*getInchFactor();   // pushed toward the top edge

	orbPosition = headPos + viewDir*forwardDist + right*rightOffset + up*upOffset;
	orbOrientation = Rotation::fromBaseVectors(right,up);

	Vislet::frame();
	Vrui::requestUpdate();
	}

// Local function to this cpp file for debugging or informational purposes: 
// returns a string representation of the OrbState enum
const char* getStateName(VoiceAssistant::OrbState state)
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
GLColor<GLfloat,4> getStateColor(VoiceAssistant::OrbState state)
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

void VoiceAssistant::applyState(OrbState newState)
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

	OrbState newState;
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

/* ///////////////////////////////////////////////////
Next chunk of code is self-contained orb rendering functions
These are vibe-coded and not finalized. Looking at other blending / overlay modes offered
in GL might be useful for a better effect, but is good starting point atm
*/ ///////////////////////////////////////////////////

void drawGradientRing(GLfloat centerX,GLfloat centerY,GLfloat innerR,GLfloat outerR,GLfloat leadPhase,GLfloat maxAlpha,GLfloat sharpness,const GLColor<GLfloat,4>& baseColor)
	{
	const int numSteps = 96;

	glBegin(GL_QUAD_STRIP);
	for(int i=0;i<=numSteps;++i)
		{
		GLfloat angle = 2.0f*Math::Constants<GLfloat>::pi*GLfloat(i)/GLfloat(numSteps);
		GLfloat t = (1.0f+Math::cos(angle-leadPhase))*0.5f; // 0..1 smooth sweep
		if(t<0.0f)
			t = 0.0f; // guard tiny float overshoot below 0 -- pow() of a negative base with a non-integer exponent is NaN
		GLfloat alpha = maxAlpha*Math::pow(t,sharpness); // higher sharpness = narrower bright band, harder edge

		glColor(GLColor<GLfloat,4>(baseColor[0],baseColor[1],baseColor[2],alpha));
		glVertex(centerX+innerR*Math::cos(angle),centerY+innerR*Math::sin(angle));
		glVertex(centerX+outerR*Math::cos(angle),centerY+outerR*Math::sin(angle));
		}
	glEnd();
	}

void drawArcRing(GLfloat centerX,GLfloat centerY,GLfloat innerR,GLfloat outerR,GLfloat startAngle,GLfloat arcSpan,GLfloat alpha,const GLColor<GLfloat,4>& color)
	{
	// Like drawGradientRing, but sweeps only [startAngle,startAngle+arcSpan) at a
	// single uniform alpha -- for a static, non-animating "broken ring" shape.
	const int numSteps = 48;
	GLColor<GLfloat,4> drawColor(color[0],color[1],color[2],alpha);

	glColor(drawColor);
	glBegin(GL_QUAD_STRIP);
	for(int i=0;i<=numSteps;++i)
		{
		GLfloat angle = startAngle+arcSpan*GLfloat(i)/GLfloat(numSteps);
		glVertex(centerX+innerR*Math::cos(angle),centerY+innerR*Math::sin(angle));
		glVertex(centerX+outerR*Math::cos(angle),centerY+outerR*Math::sin(angle));
		}
	glEnd();
	}

GLfloat pulseEnvelope(GLfloat phase)
	{
	// phase wraps into [0,1): fully on for 30%, smooth 20%-wide crossfade down,
	// fully off for 30%, smooth 20%-wide crossfade back up. Used to alternate two
	// things with a deliberate overlap window instead of an instant switch.
	phase = phase-Math::floor(phase);
	if(phase<0.3f)
		return 1.0f;
	else if(phase<0.5f)
		{
		GLfloat u = (phase-0.3f)/0.2f;
		return 1.0f-u*u*(3.0f-2.0f*u); // smoothstep down
		}
	else if(phase<0.8f)
		return 0.0f;
	else
		{
		GLfloat u = (phase-0.8f)/0.2f;
		return u*u*(3.0f-2.0f*u); // smoothstep up
		}
	}

void VoiceAssistant::display(GLContextData& contextData) const
	{
	GLfloat scale = .02f*GLfloat(getInchFactor()); // the one dial -- everything below is scale*someMultiplier
	double elapsed = getApplicationTime()-stateStartTime;

	glPushMatrix();
	glTranslate(orbPosition-Point::origin);
	glRotate(orbOrientation);

	glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE); // additive: glows over whatever's behind instead of darkening it toward black

	GLfloat ringCenterR = scale*0.9f;
	GLfloat ringHalfThickness = scale*0.16f; // 20% thinner than the original 0.2 half-thickness
	GLfloat innerR = ringCenterR-ringHalfThickness, outerR = ringCenterR+ringHalfThickness;
	GLfloat ringOffset = scale*0.35f;
	GLColor<GLfloat,4> base = getStateColor(state);

	switch(state)
		{
		case Idle:
			// Intentionally nothing -- resting, out of sight until woken
			break;

		case Warmup:
			{
			GLfloat speed = 3.0f;

			// Phase offsets chosen so each ring's transparent point starts facing AWAY
			// from the shared center, then rotates inward as elapsed increases.
			GLfloat leadPhaseA = GLfloat(elapsed)*-speed - Math::Constants<GLfloat>::pi*0.25f; // -45 deg start
			GLfloat leadPhaseB = GLfloat(elapsed)* speed + Math::Constants<GLfloat>::pi*0.75f; // +135 deg start

			drawGradientRing(-ringOffset, ringOffset,innerR,outerR,leadPhaseA,0.6f,2.5f,base);
			drawGradientRing( ringOffset,-ringOffset,innerR,outerR,leadPhaseB,1.0f,2.5f,base);
			break;
			}

		case Listening:
			{
			// Both rings breathe together in sync -- uniform brightness, no rotating sweep
			// (sharpness 0 makes drawGradientRing's t^sharpness == 1 everywhere, i.e. flat)
			GLfloat pulse = 0.4f+0.6f*(0.5f+0.5f*Math::sin(GLfloat(elapsed)*4.0f));

			drawGradientRing(-ringOffset, ringOffset,innerR,outerR,0.0f,pulse,0.0f,base);
			drawGradientRing( ringOffset,-ringOffset,innerR,outerR,0.0f,pulse,0.0f,base);
			break;
			}

		case Thinking:
			{
			// Crossfading alternation: each ring holds "on" for part of the cycle, with
			// a smooth 20%-of-period handoff overlap at each end instead of a hard cut.
			GLfloat period = 3.0f; // seconds per full alternation cycle
			GLfloat phase = GLfloat(elapsed)/period;
			GLfloat pulseA = 0.3f+0.7f*pulseEnvelope(phase);
			GLfloat pulseB = 0.3f+0.7f*pulseEnvelope(phase+0.5f);

			// Glow grows/shrinks with brightness; its half-thickness at full pulse is
			// the 20%-thinner base ring thickness, capped an extra 30% thinner on top.
			GLfloat glowCenterR = scale*0.95f;
			GLfloat glowMaxHalfThickness = scale*0.45f*0.8f*0.7f;
			GLfloat glowHalfThicknessA = glowMaxHalfThickness*pulseA;
			GLfloat glowHalfThicknessB = glowMaxHalfThickness*pulseB;

			// Soft outer glow halo, drawn first so the crisper main ring sits on top of it
			drawGradientRing(-ringOffset, ringOffset,glowCenterR-glowHalfThicknessA,glowCenterR+glowHalfThicknessA,0.0f,pulseA*0.3f,0.0f,base);
			drawGradientRing( ringOffset,-ringOffset,glowCenterR-glowHalfThicknessB,glowCenterR+glowHalfThicknessB,0.0f,pulseB*0.3f,0.0f,base);

			drawGradientRing(-ringOffset, ringOffset,innerR,outerR,0.0f,pulseA,0.0f,base);
			drawGradientRing( ringOffset,-ringOffset,innerR,outerR,0.0f,pulseB,0.0f,base);
			break;
			}

		case Speaking:
			{
			// Both rings open (grow) and close (shrink) together, same time
			GLfloat openness = 0.5f+0.5f*Math::sin(GLfloat(elapsed)*4.0f);
			GLfloat speakCenterR = scale*(0.7f+0.45f*openness);
			GLfloat speakHalfThickness = scale*(0.16f+0.04f*openness); // 20% thinner than before
			GLfloat speakInnerR = speakCenterR-speakHalfThickness;
			GLfloat speakOuterR = speakCenterR+speakHalfThickness;

			drawGradientRing(-ringOffset, ringOffset,speakInnerR,speakOuterR,0.0f,0.9f,0.0f,base);
			drawGradientRing( ringOffset,-ringOffset,speakInnerR,speakOuterR,0.0f,0.9f,0.0f,base);
			break;
			}

		case Error:
			{
			// Broken semicircle rings, constant opacity (no fading), with a slow
			// +/-10 degree wobble rotation -- mirrored 180 degrees from each other so
			// both gaps face the shared middle point instead of the same direction.
			GLfloat arcSpan = Math::Constants<GLfloat>::pi; // 180 degrees -- a semicircle
			GLfloat alpha = 0.9f; // constant -- does not fade
			GLfloat wobble = Math::Constants<GLfloat>::pi*(10.0f/180.0f)*Math::sin(GLfloat(elapsed)*0.7f); // +/-10 degrees
			// A 180-degree gap centered on "toward the shared middle" (-45 deg for this
			// ring) starts 90 degrees before that, i.e. at -45+90=45 degrees from here.
			GLfloat gapCenterOffset = Math::Constants<GLfloat>::pi*0.25f;

			drawArcRing(-ringOffset, ringOffset,innerR,outerR,gapCenterOffset+wobble,arcSpan,alpha,base);
			drawArcRing( ringOffset,-ringOffset,innerR,outerR,gapCenterOffset+Math::Constants<GLfloat>::pi+wobble,arcSpan,alpha,base);
			break;
			}
		}

	glPopAttrib();
	glPopMatrix();
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

