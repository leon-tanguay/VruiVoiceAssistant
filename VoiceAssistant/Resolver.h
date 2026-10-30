/***********************************************************************
Resolver - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 9 for what this file is
supposed to do and what's wrong with v1's version).

Parsing Ollama's HTTP reply: read ../guide/RECODE_CHECKLIST.md item 1 first.
Copy the response body (a std::string from OllamaClient) into an
IO::FixedMemoryFile(size) -- NOT the (Byte*,size) constructor, which adopts
and frees the buffer; SttEngine::transcribe() has the working pattern --
hand that to IO::JsonSource, call .parseEntity(), then
IO::getObjectProperty()/IO::getString()/IO::getNumber() to pull out
kind/command/args/speak/confidence -- not the old hand-rolled Json class.

SECURITY: read ../guide/CODE_REVIEW.md finding #1 before writing commandLine().
v1 copies argument STRING VALUES straight out of the model's JSON into a
pipe-command line with no sanitisation -- no rejection of embedded '\n',
'\r', or '"'. Because the receiving pipe protocol treats '\n' as "end of
this command, start the next one," an argument value containing a literal
newline gets a SECOND, attacker/model-influenced command dispatched for
free. Reject or strip control characters from every argument value before
it becomes part of a command line, and settle the quoting convention
against what Misc::CommandDispatcher's tokenizer actually does (don't
guess one, per DEPENDENCIES.md's own §12 lesson).
***********************************************************************/

#ifndef VOICEASSISTANT_RESOLVER_INCLUDED
#define VOICEASSISTANT_RESOLVER_INCLUDED

#include <string>

namespace VoiceAsst
{

class CommandCatalog;
class OllamaClient;

struct Resolution
{ // Ollama prompt output

	// What type of response?
	enum Kind
	{
		Nothing,
		RunCommand,
		Answer,
		Clarify
	};

	Kind kind = Nothing;
	std::string command;
	std::string args;
	std::string speak; // What should tts say to user
};

class Resolver
{
	/* Elements: */
	private:
	OllamaClient& client; // Reference to Ollama client

	/* Constructors and destructors: */
	public:
	Resolver(OllamaClient& sClient);

	/* Methods: */
	bool resolve(const CommandCatalog& catalog, const std::string& transcript, Resolution& result);
};

}

#endif
