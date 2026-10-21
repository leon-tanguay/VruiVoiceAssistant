/***********************************************************************
OllamaClient - See OllamaClient.h.
***********************************************************************/

#include "OllamaClient.h"

#include <curl/curl.h>

#include <IO/File.h>
#include <IO/FixedMemoryFile.h>
#include <IO/JsonEntityTypes.h>
#include <IO/JsonSource.h>

#include <sstream>
#include <stdexcept>
#include <string.h>

namespace VoiceAsst
{

// Anonymous namespace private to only this file
namespace
{

/***************************
Local helper functions
*****************************/
void initCurlOnce(void)
{
	static bool initialized = (curl_global_init(CURL_GLOBAL_DEFAULT) == 0);
	(void)initialized;
}

size_t appendToString(char* data, size_t size, size_t count, void* userData) // curl calls this for each chunk of the response
{
	std::string* response = static_cast<std::string*>(userData);
	response->append(data, size * count);
	return size * count; // Telling curl we took it all; anything else makes curl abort the transfer
}

IO::JsonPointer parseJson(const std::string& text) // Throws std::runtime_error if text isn't valid JSON
{
	IO::FixedMemoryFile* file = new IO::FixedMemoryFile(text.size());
	IO::FilePtr filePtr(file);
	memcpy(file->getMemory(), text.data(), text.size());
	IO::JsonSource source(filePtr);
	return source.parseEntity();
}

}

OllamaClient::OllamaClient(const std::string& sBaseUrl, const std::string& sModel, double sKeepAlive, double sTimeout) :
	baseUrl(sBaseUrl), model(sModel), keepAlive(sKeepAlive), timeoutMs(long(sTimeout * 1000.0))
{
}

bool OllamaClient::preload(void)
{
	// A chat request with no messages makes Ollama load the model and return
	IO::JsonObjectPointer request = new IO::JsonObject;
	request->setProperty("model", model);
	request->setProperty("keep_alive", keepAlive);
	IO::JsonArrayPointer messages = new IO::JsonArray;
	request->setProperty("messages", *messages);

	std::ostringstream body;
	body << *request;
	std::string response;
	return post("/api/chat", body.str(), 60000, response) == 200;
}

OllamaClient::CurlHandle::CurlHandle(void) :
	curl(nullptr)
{
	initCurlOnce();
	curl = curl_easy_init();
	if (curl == nullptr)
		throw std::runtime_error("OllamaClient: could not create a libcurl handle");
}

OllamaClient::CurlHandle::~CurlHandle(void)
{
	curl_easy_cleanup(curl);
}

long OllamaClient::post(const char* path, const std::string& body, long requestTimeoutMs, std::string& response)
{
	response.clear();
	curl_easy_reset(handle.curl); // Forget the last request's options, keep the open connection

	std::string url = baseUrl + path;
	curl_easy_setopt(handle.curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(handle.curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(handle.curl, CURLOPT_POSTFIELDSIZE, long(body.size()));

	curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
	curl_easy_setopt(handle.curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(handle.curl, CURLOPT_CONNECTTIMEOUT_MS, 500L); // Ollama is on localhost: it answers at once or isn't running
	curl_easy_setopt(handle.curl, CURLOPT_TIMEOUT_MS, requestTimeoutMs); // Limit for this request, including the model's answer
	curl_easy_setopt(handle.curl, CURLOPT_NOSIGNAL, 1L); // Don't use signals for timeouts: unsafe with threads

	curl_easy_setopt(handle.curl, CURLOPT_WRITEFUNCTION, appendToString);
	curl_easy_setopt(handle.curl, CURLOPT_WRITEDATA, &response);

	CURLcode result = curl_easy_perform(handle.curl); // Blocks until done, failed, or timed out
	curl_slist_free_all(headers);

	if (result != CURLE_OK)
		return -1;

	long status = 0;
	curl_easy_getinfo(handle.curl, CURLINFO_RESPONSE_CODE, &status);
	return status;
}

bool OllamaClient::chat(const std::string& system, const std::string& user, IO::JsonPointer schema, std::string& reply)
{
	// Build the request: {"model":..., "messages":[...], "format":<schema>, ...}
	IO::JsonObjectPointer request = new IO::JsonObject;
	request->setProperty("model", model);
	request->setProperty("stream", false);
	request->setProperty("keep_alive", keepAlive);
	request->setProperty("think", false); // qwen3's thinking only adds latency here
	request->setProperty("format", *schema);

	IO::JsonObjectPointer options = new IO::JsonObject;
	request->setProperty("options", *options);
	options->setProperty("temperature", 0.0); // Same input, same answer

	IO::JsonArrayPointer messages = new IO::JsonArray;
	request->setProperty("messages", *messages);
	IO::JsonObjectPointer systemMessage = new IO::JsonObject;
	messages->addItem(*systemMessage);
	systemMessage->setProperty("role", "system");
	systemMessage->setProperty("content", system);
	IO::JsonObjectPointer userMessage = new IO::JsonObject;
	messages->addItem(*userMessage);
	userMessage->setProperty("role", "user");
	userMessage->setProperty("content", user);

	std::ostringstream body;
	body << *request;

	// Send it; the model's answer is in message.content
	std::string response;
	if (post("/api/chat", body.str(), timeoutMs, response) != 200)
		return false;

	try
	{
		IO::JsonPointer root = parseJson(response);
		reply = IO::getString(IO::getObjectProperty(IO::getObjectProperty(root, "message"), "content"));
		return true;
	}
	catch (const std::runtime_error&)
	{
		return false;
	}
}

}
