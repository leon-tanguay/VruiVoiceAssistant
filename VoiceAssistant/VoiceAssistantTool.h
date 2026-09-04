#ifndef VRUI_VOICEASSISTANTTOOL_INCLUDED
#define VRUI_VOICEASSISTANTTOOL_INCLUDED

#include <Vrui/UtilityTool.h>

/********************************************
Lightweight and simple tool added to Vrui that sends the press and release 
commands to the voice assistant when the tool's button is pressed and released. 
This allows the user to control the voice assistant with a single button on 
any input device.
********************************************/
namespace Vrui 
{

class VoiceAssistantTool;

class VoiceAssistantToolFactory:public ToolFactory
	{
	friend class VoiceAssistantTool;

	/* Constructors and destructors: */
	public:
	VoiceAssistantToolFactory(ToolManager& toolManager);
	virtual ~VoiceAssistantToolFactory(void);

	/* Methods from ToolFactory: */
	virtual const char* getName(void) const;
	virtual const char* getButtonFunction(int buttonSlotIndex) const;
	virtual Tool* createTool(const ToolInputAssignment& inputAssignment) const;
	virtual void destroyTool(Tool* tool) const;
	};

class VoiceAssistantTool:public UtilityTool
	{
	friend class VoiceAssistantToolFactory;

	/* Elements: */
	private:
	static VoiceAssistantToolFactory* factory;

	/* Constructors and destructors: */
	public:
	VoiceAssistantTool(const ToolFactory* factory,const ToolInputAssignment& inputAssignment);

	/* Methods from Tool: */
	virtual const ToolFactory* getFactory(void) const;
	virtual void buttonCallback(int buttonSlotIndex,InputDevice::ButtonCallbackData* cbData);
	};

}

#endif