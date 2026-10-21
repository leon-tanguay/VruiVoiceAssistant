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

#include <IO/JsonEntity.h>

#include <string>

namespace VoiceAsst
{

class OllamaClient
{ // Not thread-safe: call every method from the same thread (the worker)

	private:
	/* Private member struct*/
	struct CurlHandle // Owns a libcurl easy handle; frees it on destruction
	{
		void* curl; // A CURL*; curl.h defines CURL as void for client code

		CurlHandle(void); // Creates the handle; throws std::runtime_error on failure
		~CurlHandle(void); // Calls curl_easy_cleanup

		private:
		CurlHandle(const CurlHandle& source); // Prohibit copy constructor
		CurlHandle& operator=(const CurlHandle& source); // Prohibit assignment
	};

	std::string baseUrl; // ex http://127.0.0.1:11434
	std::string model; // ex qwen3:4b
	double keepAlive; // How long Ollama keeps model loaded after a request, ex 30m
	CurlHandle handle;
	long timeoutMs;

	long post(const char* path, const std::string& body, long requestTimeoutMs, std::string& response); // HTTP status, or -1 if Ollama couldn't be reached in time

	public:
	OllamaClient(const std::string& sBaseUrl, const std::string& sModel, double sKeepAlive, double sTimeout);

	bool preload(void); // Loads the model into memory now, so the first real request doesn't pay for it
	bool chat(const std::string& system, const std::string& user, IO::JsonPointer schema, std::string& reply); // reply = the model's JSON text; false if Ollama failed
};
}

#endif
