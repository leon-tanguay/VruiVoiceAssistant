/***********************************************************************
Resolver - See Resolver.h.
***********************************************************************/

#include "Resolver.h"

#include "CommandCatalog.h"
#include "OllamaClient.h"

#include <ctype.h>
#include <string.h>

#include <stdexcept>

#include <IO/File.h>
#include <IO/FixedMemoryFile.h>
#include <IO/JsonEntityTypes.h>
#include <IO/JsonSource.h>

namespace VoiceAsst
{

// Anonymous namespace private to only this file
namespace
{

/***************************
System prompt sent to Ollama, adapted from v1's (old-vrui-assistant/VoiceAssistant/Resolver.cpp).
The live command list is appended after it as "COMMANDS:" with one "id | arguments | description" line each.
*****************************/
const char* const systemPrompt =
	"You are the voice assistant inside Vrui, a virtual-reality application. The user is wearing\n"
	"a VR headset and talks to you; they cannot see a keyboard or a console, and everything you\n"
	"put in `speak` is read aloud to them.\n"
	"\n"
	"The REQUEST is an automatic speech-to-text transcript and may contain recognition\n"
	"errors or near-homophones (e.g. \"recent\" -> \"reset\", \"camera\" -> \"view\", \"safe\" ->\n"
	"\"save\"). Infer the intended meaning, and map a slightly misheard request to the\n"
	"closest matching command instead of rejecting it.\n"
	"\n"
	"COMMANDS lists the only commands that exist, one per line as `id | arguments | description`.\n"
	"In arguments, <angle> is required and [square] is optional.\n"
	"\n"
	"- If one command does what the user wants: set `command` to its id, `kind` to \"command\",\n"
	"  `args` to the argument text exactly as it would follow the command (\"\" if none), and\n"
	"  `speak` to \"\". Fill every required argument; if you can't, ask instead.\n"
	"- If the user asks a question (\"what can you do?\", \"how do I save my view?\"): set\n"
	"  `command` to \"none\", `kind` to \"answer\", and answer in `speak` in one or two short\n"
	"  spoken sentences. Mention only commands from COMMANDS. If something isn't possible, say\n"
	"  so plainly.\n"
	"- If the request is unclear or missing information: set `command` to \"none\", `kind` to\n"
	"  \"clarify\", and ask one short question in `speak`.\n"
	"\n"
	"When you explain how to do something, tell the user what to SAY, in plain words (e.g.\n"
	"say \"reset the view\"). Never mention command ids, argument syntax, typing, or the console.\n"
	"Be brief: no greetings, no filler, no lists.\n"
	"\n"
	"Never invent commands or arguments.\n";

IO::JsonPointer parseJson(const std::string& text) // Throws std::runtime_error if text isn't valid JSON
{
	IO::FixedMemoryFile* file = new IO::FixedMemoryFile(text.size());
	IO::FilePtr filePtr(file);
	memcpy(file->getMemory(), text.data(), text.size());
	IO::JsonSource source(filePtr);
	return source.parseEntity();
}

}

Resolver::Resolver(OllamaClient& sClient) :
	client(sClient) {}

bool Resolver::resolve(const CommandCatalog& catalog, const std::string& transcript, Resolution& result)
{
	result = Resolution();

	std::string text = transcript;

	// Remove leading whitespace
	while (!text.empty() && isspace((unsigned char)text[0]))
		text.erase(0, 1);

	// Remove trailing whitespace
	while (!text.empty() && isspace((unsigned char)text.back()))
		text.pop_back();

	if (text.empty()) return true;

	// System message: the rules, then the live command list
	std::string system = systemPrompt;
	system += "\nCOMMANDS:\n";
	system += catalog.getPromptText();

	std::string reply;
	if (!client.chat(system, text, catalog.makeSchema(), reply)) return false;

	try
	{
		IO::JsonPointer root = parseJson(reply);
		const std::string kind = IO::getString(IO::getObjectProperty(root, "kind"));
		result.command = IO::getString(IO::getObjectProperty(root, "command"));
		result.args = IO::getString(IO::getObjectProperty(root, "args"));
		result.speak = IO::getString(IO::getObjectProperty(root, "speak"));

		// A "command" reply that names no command means nothing to run
		if (kind == "command" && result.command != "none")
			result.kind = Resolution::RunCommand;
		else if (kind == "answer")
			result.kind = Resolution::Answer;
		else if (kind == "clarify")
			result.kind = Resolution::Clarify;
	}
	catch (const std::runtime_error&)
	{
		result = Resolution();
		return false;
	}
	return true;
}

}
