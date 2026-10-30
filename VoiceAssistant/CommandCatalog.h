/***********************************************************************
CommandCatalog - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 4 for what this file is
supposed to do and what's wrong with v1's version).

Loading VoiceAssistant-catalog.json: read ../guide/RECODE_CHECKLIST.md
item 1 first. IO::JsonSource("VoiceAssistant-catalog.json").parseEntity()
gets you the root IO::JsonPointer; IO::getObjectProperty(root,"commands")
+ IO::getArray(...) walks the "commands" array the same way v1's hand-rolled
Json did, just with Vrui's own type.
***********************************************************************/

#ifndef VOICEASSISTANT_COMMANDCATALOG_INCLUDED
#define VOICEASSISTANT_COMMANDCATALOG_INCLUDED

#include <string>
#include <vector>

#include <IO/JsonEntity.h>

namespace Misc
{
// Trick to use CommandDispatcher without including it in header
class CommandDispatcher;

}

namespace VoiceAsst
{

struct Command
{
	std::string id;
	std::string arguments;
	std::string description;
};

class CommandCatalog
{
	/* Elements: */
	private:
	std::vector<Command> catalog;

	/* Methods: */
	public:
	// Too simple for need of a constructor
	void fillCommands(const Misc::CommandDispatcher& commandDispatcher); // Called on main thread, as is not thread safe, passed into sttThread
	const std::vector<Command>& getCommands(void) const; // Gets list of commands
	std::string getPromptText(void) const; // Returns command list as text for llm prompts
	IO::JsonPointer makeSchema(void) const; // Forces Ollama to follow pecific format
};

}

#endif
