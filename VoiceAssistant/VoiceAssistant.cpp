/***********************************************************************
VoiceAssistant - See VoiceAssistant.h.
***********************************************************************/

#include "VoiceAssistant.h"

#include <GL/gl.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLColorTemplates.h>

#include <Math/Math.h>

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
	:Vislet()
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
	}

void VoiceAssistant::display(GLContextData& contextData) const
	{
	GLfloat orbRadius = .02f*GLfloat(getInchFactor());
	const int numSides = 60;

	glPushMatrix();
	glTranslate(orbPosition-Point::origin);
	glRotate(orbOrientation);
	
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_LIGHTING);

	glColor(GLColor<GLfloat,4>(1.0f,0.85f,0.0f)); // yellow, once for the whole fan
	glBegin(GL_TRIANGLE_FAN);
	glVertex(0.0f,0.0f); // center
	for(int i=0;i<=numSides;++i)
		{
		GLfloat angle = 2.0f*Math::Constants<GLfloat>::pi*GLfloat(i)/GLfloat(numSides);
		glVertex(orbRadius*Math::cos(angle),orbRadius*Math::sin(angle));
		}
	glEnd();
	glPopAttrib();
	glPopMatrix();
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
	}

// Local function to this cpp file for debugging or informational purposes: 
// returns a string representation of the OrbState enum
const char* getStateName(VoiceAssistant::OrbState state)
	{
	switch(state)
		{
		case VoiceAssistant::Warming:
			return "Warming";
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

void VoiceAssistant::applyState(OrbState newState)
	{
	if(this->state != newState)
		{
		state = newState;
		stateStartTime = getApplicationTime();

		// Use Vrui message logger to log state change without need for std::string
		Misc::formattedConsoleNote("VoiceAssistant: state changed to %s",getStateName(state));
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

