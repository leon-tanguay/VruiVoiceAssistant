/***********************************************************************
Json - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 1 for what this file is
supposed to do and what's wrong with v1's version).

STOP -- read this before writing a single line here.

v1 hand-rolled this whole file (a Type enum, a tagged union of
bool/double/string/vector/map, a recursive-descent parser) because it
assumed Vrui had no JSON support. That assumption was wrong. Vrui/IO
ships a real, RFC-8259 JSON implementation:

    <IO/JsonEntity.h>       -- IO::JsonEntity, the abstract value type
                               (BOOLEAN/NUMBER/STRING/ARRAY/OBJECT),
                               reference-counted via Misc::Autopointer
                               (IO::JsonPointer), the SAME smart-pointer
                               style as the rest of Vrui/Misc
    <IO/JsonEntityTypes.h>  -- the concrete classes: IO::JsonBoolean,
                               IO::JsonNumber, IO::JsonString, IO::JsonArray
                               (a std::vector<JsonPointer>), IO::JsonObject
                               (a Misc::HashTable<std::string,JsonPointer>),
                               plus free helper functions IO::getNumber(),
                               IO::getString(), IO::getArray(),
                               IO::getObjectProperty(), etc.
    <IO/JsonSource.h>       -- IO::JsonSource: JsonSource(const char*
                               fileName) or JsonSource(IO::FilePtr) or
                               JsonSource(IO::File&), then .parseEntity()
                               returns a JsonPointer. Takes a FILE NAME
                               directly (for the catalog) OR any IO::File,
                               including an in-memory one -- see
                               <IO/FixedMemoryFile.h>, which wraps an
                               existing byte buffer as a readable IO::File
                               -- which is exactly how to parse an
                               in-memory string (Ollama's HTTP response
                               body, a Vosk result string) instead of a
                               file on disk.

This covers every single thing v1's Json.cpp did: the bundled catalog
(CommandCatalog.h, item 4), Ollama's HTTP replies (Resolver.h, item 9),
and Vosk's JSON result strings (SttEngine.h, item 6) -- all three, with
Vrui's own type, not a fourth hand-rolled one.

So: this file most likely should not exist at all in the finished
recode. #include <IO/JsonEntity.h> etc. directly wherever a JSON value is
needed, the same way you'd #include <Vrui/DisplayState.h> directly rather
than writing a wrapper around it. If something IS still worth keeping
here, it's a MUCH smaller thing than v1's Json.cpp: at most a couple of
tiny free functions that keep showing up at every call site (decide once
you're actually writing CommandCatalog/Resolver/SttEngine whether that's
true, or whether it's premature -- three call sites isn't a strong signal
either way for a helper). Update ../guide/RECODE_CHECKLIST.md's own item 1
and the checklist file count once you've made that call.
***********************************************************************/

#ifndef VOICEASSISTANT_JSON_INCLUDED
#define VOICEASSISTANT_JSON_INCLUDED

namespace VoiceAsst {

// TODO: declare Json (../guide/RECODE_CHECKLIST.md item 1).

}

#endif
