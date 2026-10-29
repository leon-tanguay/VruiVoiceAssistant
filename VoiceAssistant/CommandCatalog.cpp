/***********************************************************************
CommandCatalog - See CommandCatalog.h.
***********************************************************************/

#include "CommandCatalog.h"

// C strings used here
#include <string.h>

#include <algorithm>
#include <Misc/CommandDispatcher.h>

#include <IO/JsonEntityTypes.h>

namespace VoiceAsst
{

// Anonymous namespace (only visible within file)
namespace
{
// Blacklisted commands (hardcoded for safety)
const char* const hiddenPrefixes[] = {
	"voiceAssistant.", "help", "listCommands",
	"VisletManager::", "Window(", "Screen(", "Viewer(",
	"loadInputGraph", "setFrontplaneDist", "setBackplaneDist"};

bool isHidden(const std::string& id)
{
	// Divides total size of hidden prefixes by size of each pointer to get # of entries
	int hiddenPrefixesLen = sizeof(hiddenPrefixes) / sizeof(hiddenPrefixes[0]);

	for (int i = 0; i < hiddenPrefixesLen; ++i)
	{
		if (id.compare(0, strlen(hiddenPrefixes[i]), hiddenPrefixes[i]) == 0) return true;
	}
	return false;
}

bool commandLess(const Command& a, const Command& b)
{
	return a.id < b.id;
}
}

void CommandCatalog::fillCommands(const Misc::CommandDispatcher& commandDispatcher)
{
	// Reset catalog
	catalog.clear();

	// Get command map
	const Misc::CommandDispatcher::CommandMap& commandMap = commandDispatcher.getCommandMap();

	for (Misc::CommandDispatcher::CommandMap::ConstIterator it = commandMap.begin(); !it.isFinished(); ++it)
	{
		const auto& itSource = it->getSource();

		// Do nothing if blacklisted
		if (isHidden(itSource)) continue;

		// Make new command
		Command cmd;
		cmd.id = itSource;

		const auto& itDest = it->getDest();
		cmd.arguments = itDest.getArguments();
		cmd.description = itDest.getDescription();

		// Add command to catalog
		catalog.push_back(cmd);
	}

	// Sort catalog alphabetically
	std::sort(catalog.begin(), catalog.end(), commandLess);
}

const std::vector<Command>& CommandCatalog::getCommands(void) const
{
	return catalog;
}

std::string CommandCatalog::getPromptText(void) const
{
	// Format: id | arguments | description (for each line)
	std::string s;
	for (const Command& command : catalog)
	{
		s += command.id + " | " + command.arguments + " | " + command.description + "\n";
	}
	return s;
}

IO::JsonPointer CommandCatalog::makeSchema(void) const
{
	/*
    {
        "type": "object",
        "properties": {
            "command": { "type": "string", "enum": ["loadView", "quit", "resetView", "...", "none"] },
            "args":    { "type": "string" },
            "kind":    { "type": "string", "enum": ["command", "answer", "clarify"] },
            "speak":   { "type": "string" }
        },
        "required": ["command", "args", "kind", "speak"]
    }
    
    */
	// JSON entities are reference counted: create them with new and hold them in their Autopointer types,
	// never on the stack, or the parent holding them would try to delete a stack object.
	// Note: JsonObject prints its properties in hash order, not in the order they're set here

	// "command": one of the catalog's ids, or "none" for answers and clarifications
	IO::JsonArrayPointer commandEnum = new IO::JsonArray;
	for (const Command& command : catalog)
	{
		commandEnum->addItem(command.id);
	}
	commandEnum->addItem(std::string("none")); // std::string, or the bool overload is picked

	IO::JsonObjectPointer commandProperty = new IO::JsonObject;
	commandProperty->setProperty("type", "string");
	commandProperty->setProperty("enum", *commandEnum);

	// "args": the argument text exactly as it would follow the command; "" if none
	IO::JsonObjectPointer argsProperty = new IO::JsonObject;
	argsProperty->setProperty("type", "string");

	// "kind": what sort of reply this is
	IO::JsonArrayPointer kindEnum = new IO::JsonArray;
	kindEnum->addItem(std::string("command"));
	kindEnum->addItem(std::string("answer"));
	kindEnum->addItem(std::string("clarify"));

	IO::JsonObjectPointer kindProperty = new IO::JsonObject;
	kindProperty->setProperty("type", "string");
	kindProperty->setProperty("enum", *kindEnum);

	// "speak": what to say out loud; "" for plain commands
	IO::JsonObjectPointer speakProperty = new IO::JsonObject;
	speakProperty->setProperty("type", "string");

	// Assemble the properties and the list of required fields
	IO::JsonObjectPointer properties = new IO::JsonObject;
	properties->setProperty("command", *commandProperty);
	properties->setProperty("args", *argsProperty);
	properties->setProperty("kind", *kindProperty);
	properties->setProperty("speak", *speakProperty);

	IO::JsonArrayPointer required = new IO::JsonArray;
	required->addItem(std::string("command"));
	required->addItem(std::string("args"));
	required->addItem(std::string("kind"));
	required->addItem(std::string("speak"));

	// The whole reply must be one object with all four fields
	IO::JsonObjectPointer schema = new IO::JsonObject;
	schema->setProperty("type", "object");
	schema->setProperty("properties", *properties);
	schema->setProperty("required", *required);

	return schema;
}

}
