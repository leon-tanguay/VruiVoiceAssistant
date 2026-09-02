/***********************************************************************
OllamaClient - TODO: write the banner comment (see ../guide/STYLE_GUIDE.md section 2 for
the format, and ../guide/RECODE_CHECKLIST.md item 7 for what this file is
supposed to do and what's wrong with v1's version).

libcurl vs Vrui's own Comm:: package -- checked, worth knowing: Vrui does
ship real HTTP client code (<Comm/HttpFile.h>, <Comm/HttpDirectory.h>,
built on <Comm/TCPPipe.h>), but it's GET-only (reading a remote file/URL).
<Comm/HttpPostRequest.h> looks POST-related by name but is the opposite of
what's needed here -- it PARSES an incoming POST for something acting as
an HTTP server, not sends one. There's no ready client-side "POST this
JSON body" helper, so doing this "the Vrui way" means hand-framing the
HTTP/1.1 POST request on top of Comm::TCPPipe yourself (headers, body,
reading a possibly-chunked response back) -- mirroring what HttpFile does
internally for GET. That's real, doable, and more consistent with "Vrui
prefers its own I/O primitives over an external library" -- but it's
materially more code and risk than libcurl for zero user-facing benefit,
since libcurl is already a completely standard system dependency (see
../guide/DEPENDENCIES.md #7). Your call: libcurl is the pragmatic default,
raw Comm::TCPPipe framing is a legitimate "resharpen your C" exercise, not
a "v1 did this wrong" fix like the pipe/JSON issues elsewhere.
***********************************************************************/

#ifndef VOICEASSISTANT_OLLAMACLIENT_INCLUDED
#define VOICEASSISTANT_OLLAMACLIENT_INCLUDED

namespace VoiceAsst {

// TODO: declare OllamaClient (../guide/RECODE_CHECKLIST.md item 7).

}

#endif
