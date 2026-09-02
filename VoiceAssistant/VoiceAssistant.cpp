/***********************************************************************
VoiceAssistant - See VoiceAssistant.h.
***********************************************************************/

#include "VoiceAssistant.h"

#include <GL/gl.h>
#include <GL/GLModels.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLMaterialTemplates.h>

#include <Vrui/Vrui.h>
#include <Vrui/Viewer.h>
#include <Vrui/VisletManager.h>

#include <Misc/MessageLogger.h>
// TODO: for display(), #include <Vrui/Vrui.h> (for getMainViewer(),
// goToPhysicalSpace()) and read the "DRAWING THE ORB -- CORRECTED" reference
// block at the top of VoiceAssistant.h before writing it. Short version: this
// is NOT a 2D/glOrtho overlay (that was an earlier, wrong draft of this
// comment) -- draw the orb as a real flat 3D object positioned in physical
// space relative to Vrui::getMainViewer()->getHeadPosition(), via
// Vrui::goToPhysicalSpace(contextData).

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

	Point headPos = getMainViewer()->getHeadPosition();
	Vector viewDir = getMainViewer()->getViewDirection();
	Vector up = getUpDirection();

	Scalar armLength = Scalar(28)*getInchFactor();   // ~arm's length
	Scalar rightOffset = Scalar(5)*getInchFactor();
	Scalar heightOffset = Scalar(3)*getInchFactor();

	Vector right = viewDir ^ up;
	orbPosition = headPos + viewDir*armLength + right*rightOffset + up*heightOffset;
	orbOrientation = Rotation::fromBaseVectors(right,up);
	}

void VoiceAssistant::display(GLContextData& contextData) const
	{
	glPushMatrix();
	glTranslate(orbPosition-Point::origin);
	glRotate(orbOrientation);

	glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,0.85f,0.0f)); // yellow ball

	GLfloat orbRadius=2.2f*GLfloat(getInchFactor()); // ~4.4in across, a bit bigger than a softball
	GLsizei orbNumStrips=12;
	glDrawSphereIcosahedron(orbRadius,orbNumStrips);

	glPopMatrix();
	}

void VoiceAssistant::disable(bool shutdown)
	{
	Vislet::disable(shutdown);
	Misc::consoleNote("VoiceAssistant: stopping");
	}
	
void VoiceAssistant::frame(void)
	{
	Vislet::frame();
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

