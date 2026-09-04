#include "VoiceAssistantTool.h"

#include <Misc/CommandDispatcher.h>

#include <Vrui/ToolManager.h>
#include <Vrui/Vrui.h>

namespace Vrui 
{

/*******************************************
Methods of class VoiceAssistantToolFactory:
*******************************************/

VoiceAssistantToolFactory::VoiceAssistantToolFactory(ToolManager& toolManager)
	:ToolFactory("VoiceAssistantTool",toolManager)
	{
	layout.setNumButtons(1);

	ToolFactory* utilityToolFactory=toolManager.loadClass("UtilityTool");
	utilityToolFactory->addChildClass(this);
	addParentClass(utilityToolFactory);

	VoiceAssistantTool::factory=this;
	}

VoiceAssistantToolFactory::~VoiceAssistantToolFactory(void)
	{
	VoiceAssistantTool::factory=0;
	}

const char* VoiceAssistantToolFactory::getName(void) const
	{
	return "Voice Assistant Push-to-Talk";
	}

const char* VoiceAssistantToolFactory::getButtonFunction(int) const
	{
	return "Push-to-Talk";
	}

Tool* VoiceAssistantToolFactory::createTool(const ToolInputAssignment& inputAssignment) const
	{
	return new VoiceAssistantTool(this,inputAssignment);
	}

void VoiceAssistantToolFactory::destroyTool(Tool* tool) const
	{
	delete tool;
	}

extern "C" void resolveVoiceAssistantToolDependencies(Plugins::FactoryManager<ToolFactory>& manager)
	{
	manager.loadClass("UtilityTool");
	}

extern "C" ToolFactory* createVoiceAssistantToolFactory(Plugins::FactoryManager<ToolFactory>& manager)
	{
	ToolManager* toolManager=static_cast<ToolManager*>(&manager);
	return new VoiceAssistantToolFactory(*toolManager);
	}

extern "C" void destroyVoiceAssistantToolFactory(ToolFactory* factory)
	{
	delete factory;
	}

/********************************************
Static elements of class VoiceAssistantTool:
********************************************/
VoiceAssistantToolFactory* VoiceAssistantTool::factory=0;

/*********************************
Methods of class VoiceAssistantTool:
*********************************/

VoiceAssistantTool::VoiceAssistantTool(const ToolFactory* factory,const ToolInputAssignment& inputAssignment)
	:UtilityTool(factory,inputAssignment)
	{
	}

const ToolFactory* VoiceAssistantTool::getFactory(void) const
	{
	return factory;
	}

void VoiceAssistantTool::buttonCallback(int buttonSlotIndex,InputDevice::ButtonCallbackData* cbData)
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

}