#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

#include <string>
#include <vector>
#include <functional>
#include "Data.h"
#include "libHttpClient.h"

class HttpResponse;

typedef std::function<void(HttpResponse* response)> ccHttpRequestCallback;

class EXPORTS_API HttpRequest {
public:
	enum class Type {
		emGET, emPOST, emPUT, emDELETE, emUNKNOWN,
	};

	HttpRequest() {
		_requestType = Type::emUNKNOWN;
		_url.clear();
		_requestData.clear();
		_tag.clear();
		_pCallback = nullptr;
		_pUserData = nullptr;
	}
	;

	/** Destructor. */
	virtual ~HttpRequest() {
	}
	;

	inline void setRequestType(Type type) {
		_requestType = type;
	}
	;

	inline Type getRequestType() {
		return _requestType;
	}
	;

	/**
	 * Set the url address of HttpRequest object.
	 * The url value could be like these: "http://httpbin.org/ip" or "https://httpbin.org/get"
	 *
	 * @param url the string pointer.
	 */
	inline void setUrl(const char* url) {
		_url = url;
	}
	;

	inline const char* getUrl() {
		return _url.c_str();
	}
	;

	inline void setRequestData(const char* buffer, size_t len) {
		_requestData.assign(buffer, buffer + len);
	}
	;

	inline char* getRequestData() {
		if (_requestData.size() != 0)
			return &(_requestData.front());

		return nullptr;
	}

	inline ssize_t getRequestDataSize() {
		return _requestData.size();
	}

	/**
	 * Set a string tag to identify your request.
	 * This tag can be found in HttpResponse->getHttpRequest->getTag().
	 *
	 * @param tag the string pointer
	 */
	inline void setTag(const char* tag) {
		_tag = tag;
	}
	;
	/**
	 * Get the string tag to identify the request.
	 * The best practice is to use it in your MyClass::onMyHttpRequestCompleted(sender, HttpResponse*) callback.
	 *
	 * @return const char* the pointer of _tag
	 */
	inline const char* getTag() {
		return _tag.c_str();
	}
	;

	/**
	 * Set user-customed data of HttpRequest object.
	 * You can attach a customed data in each request, and get it back in response callback.
	 * But you need to new/delete the data pointer manully.
	 *
	 * @param pUserData the string pointer
	 */
	inline void setUserData(void* pUserData) {
		_pUserData = pUserData;
	}
	;
	/**
	 * Get the user-customed data pointer which were pre-setted.
	 * Don't forget to delete it. HttpClient/HttpResponse/HttpRequest will do nothing with this pointer.
	 *
	 * @return void* the pointer of user-customed data.
	 */
	inline void* getUserData() {
		return _pUserData;
	}
	;

	/**
	 * Set response callback function of HttpRequest object.
	 * When response come back, we would call _pCallback to process response data.
	 *
	 * @param callback the ccHttpRequestCallback function.
	 */
	inline void setResponseCallback(const ccHttpRequestCallback& callback) {
		_pCallback = callback;
	}

	/**
	 * Get ccHttpRequestCallback callback function.
	 *
	 * @return const ccHttpRequestCallback& ccHttpRequestCallback callback function.
	 */
	inline const ccHttpRequestCallback& getCallback() {
		return _pCallback;
	}

	/**
	 * Set custom-defined headers.
	 *
	 * @param pHeaders the string vector of custom-defined headers.
	 */
	inline void setHeaders(std::vector<std::string> pHeaders) {
		_headers = pHeaders;
	}

	/**
	 * Get custom headers.
	 *
	 * @return std::vector<std::string> the string vector of custom-defined headers.
	 */
	inline std::vector<std::string> getHeaders() {
		return _headers;
	}

protected:
	// properties
	Type _requestType;    /// kHttpRequestGet, kHttpRequestPost or other enums
	std::string _url;            /// target url that this request is sent to
	std::vector<char> _requestData;    /// used for POST
	std::string _tag; /// user defined tag, to identify different requests in response callback
	ccHttpRequestCallback _pCallback;      /// C++11 style callbacks
	void* _pUserData;      /// You can add your customed data here
	std::vector<std::string> _headers;		      /// custom http headers
};

#endif //__HTTP_REQUEST_H__
