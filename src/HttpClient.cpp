#include "HttpClient.h"
#include "FileUtils.h"

#include <queue>
#include <errno.h>
#include <curl/curl.h>

typedef int int32_t;

static HttpClient* _httpClient = nullptr; // pointer to singleton

typedef size_t (*write_callback)(void *ptr, size_t size, size_t nmemb,
		void *stream);

static int debug_callback(CURL *handle, /* the handle/transfer this concerns */
curl_infotype type, /* what kind of data */
char *data, /* points to the data */
size_t size, /* size of the data pointed to */
void *userptr) /* whatever the user please */
{
	return 0;
}

int progressCallback(void *clientp, double dltotal, double dlnow,
		double ultotal, double ulnow) {
	return 0;
}

// Callback function used by libcurl for collect response data
static size_t writeData(void *ptr, size_t size, size_t nmemb, void *stream) {
	std::vector<char> *recvBuffer = (std::vector<char>*) stream;
	size_t sizes = size * nmemb;

	// add data to the end of recvBuffer
	// write data maybe called more than once in a single request
	recvBuffer->insert(recvBuffer->end(), (char*) ptr, (char*) ptr + sizes);

	return sizes;
}

// Callback function used by libcurl for collect header data
static size_t writeHeaderData(void *ptr, size_t size, size_t nmemb,
		void *stream) {
	std::vector<char> *recvBuffer = (std::vector<char>*) stream;
	size_t sizes = size * nmemb;

	// add data to the end of recvBuffer
	// write data maybe called more than once in a single request
	recvBuffer->insert(recvBuffer->end(), (char*) ptr, (char*) ptr + sizes);

	return sizes;
}

static int processGetTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void *stream, long *errorCode,
		write_callback headerCallback, void *headerStream, char* errorBuffer);
static int processPostTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void *stream, long *errorCode,
		write_callback headerCallback, void *headerStream, char* errorBuffer);
static int processPutTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void *stream, long *errorCode,
		write_callback headerCallback, void *headerStream, char* errorBuffer);
static int processDeleteTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void *stream, long *errorCode,
		write_callback headerCallback, void *headerStream, char* errorBuffer);

// Worker thread
void HttpClient::networkThread() {
	increaseThreadCount();

	while (true) {
		HttpRequest *request;
		// step 1: send http request if the requestQueue isn't empty
		{
			std::lock_guard<std::mutex> lock(_requestQueueMutex);
			while (_requestQueue.empty()) {
				_sleepCondition.wait(_requestQueueMutex);
			}
			request = _requestQueue.front();
			_requestQueue.pop();
		}

		if (request == _requestSentinel) {
			break;
		}

		// step 2: libcurl sync access

		// Create a HttpResponse object, the default setting is http access failed
		HttpResponse *response = new (std::nothrow) HttpResponse(request);
		processResponse(response, _responseMessage);

		// add response packet into queue
//         _responseQueueMutex.lock();
//         _responseQueue.push(response);
//         _responseQueueMutex.unlock();

		_schedulerMutex.lock();

		const ccHttpRequestCallback& callback = request->getCallback();
		if (callback != nullptr) {
			callback(response);
		}

		delete request;
		delete response;

// 		if (nullptr != _scheduler)
// 		{
// 			_scheduler->performFunctionInCocosThread(CC_CALLBACK_0(HttpClient::dispatchResponseCallbacks, this));
// 		}

		_schedulerMutex.unlock();
	}

	// cleanup: if worker thread received quit signal, clean up un-completed request queue
	_requestQueueMutex.lock();
	std::queue<HttpRequest*>().swap(_requestQueue);
	_requestQueueMutex.unlock();

	//_responseQueueMutex.lock();
	//std::queue<HttpResponse*>().swap(_responseQueue);
	//_responseQueueMutex.unlock();

	decreaseThreadCountAndMayDeleteThis();
}

// Worker thread
void HttpClient::networkThreadAlone(HttpRequest* request,
		HttpResponse* response) {
	increaseThreadCount();

	char responseMessage[RESPONSE_BUFFER_SIZE] = { 0 };
	processResponse(response, responseMessage);

	_schedulerMutex.lock();

	const ccHttpRequestCallback& callback = request->getCallback();
	if (callback != nullptr) {
		callback(response);
	}
	_schedulerMutex.unlock();

	delete request;
	delete response;

	decreaseThreadCountAndMayDeleteThis();
}

//Configure curl's timeout property
static bool configureCURL(HttpClient* client, CURL* handle, char* errorBuffer) {
	if (!handle) {
		return false;
	}

	int32_t code;
	code = curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errorBuffer);
	if (code != CURLE_OK) {
		return false;
	}
	code = curl_easy_setopt(handle, CURLOPT_TIMEOUT,
			HttpClient::getInstance()->getTimeoutForRead());
	if (code != CURLE_OK) {
		return false;
	}
	code = curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT,
			HttpClient::getInstance()->getTimeoutForConnect());
	if (code != CURLE_OK) {
		return false;
	}

	std::string sslCaFilename = client->getSSLVerification();
	if (sslCaFilename.empty()) {
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
	} else {
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(handle, CURLOPT_CAINFO, sslCaFilename.c_str());
	}

	// FIXED #3224: The subthread of CCHttpClient interrupts main thread if timeout comes.
	// Document is here: http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTNOSIGNAL
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

	curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");

	return true;
}

class CURLRaii {
	/// Instance of CURL
	CURL *_curl;
	/// Keeps custom header data
	curl_slist *_headers;
public:
	CURLRaii() :
			_curl(curl_easy_init()), _headers(nullptr) {
	}

	~CURLRaii() {
		if (_curl)
			curl_easy_cleanup(_curl);
		/* free the linked list for header data */
		if (_headers)
			curl_slist_free_all(_headers);
	}

	template<class T>
	bool setOption(CURLoption option, T data) {
		return CURLE_OK == curl_easy_setopt(_curl, option, data);
	}

	/**
	 * @brief Inits CURL instance for common usage
	 * @param request Null not allowed
	 * @param callback Response write callback
	 * @param stream Response write stream
	 */
	bool init(HttpClient* client, HttpRequest* request, write_callback callback,
			void* stream, write_callback headerCallback, void* headerStream,
			char* errorBuffer) {
		if (!_curl)
			return false;
		if (!configureCURL(client, _curl, errorBuffer))
			return false;

		/* get custom header data (if set) */
		std::vector<std::string> headers = request->getHeaders();
		if (!headers.empty()) {
			/* append custom headers one by one */
			for (std::vector<std::string>::iterator it = headers.begin();
					it != headers.end(); ++it)
				_headers = curl_slist_append(_headers, it->c_str());
			/* set custom headers for curl */
			if (!setOption(CURLOPT_HTTPHEADER, _headers))
				return false;
		}

		std::string cookieFilename = client->getCookieFilename();
		if (!cookieFilename.empty()) {
			if (!setOption(CURLOPT_COOKIEFILE, cookieFilename.c_str())) {
				return false;
			}
			if (!setOption(CURLOPT_COOKIEJAR, cookieFilename.c_str())) {
				return false;
			}
		}

# ifdef defined (_DEBUG) || defined (DEBUG)
		setOption(CURLOPT_DEBUGFUNCTION, debug_callback);
		setOption(CURLOPT_VERBOSE, 1);
# endif

		return setOption(CURLOPT_URL, request->getUrl())
				//&& setOption(CURLOPT_PROGRESSFUNCTION, curlProgressCallback)
				&& setOption(CURLOPT_WRITEFUNCTION, callback)
				&& setOption(CURLOPT_WRITEDATA, stream)
				&& setOption(CURLOPT_HEADERFUNCTION, headerCallback)
				&& setOption(CURLOPT_HEADERDATA, headerStream);
	}

	/// @param responseCode Null not allowed
	bool perform(long *responseCode) {
		if (CURLE_OK != curl_easy_perform(_curl))
			return false;
		CURLcode code = curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE,
				responseCode);
		if (code != CURLE_OK
				|| !(*responseCode >= 200 && *responseCode < 300)) {
			// CCLOGERROR("Curl curl_easy_getinfo failed: %s", curl_easy_strerror(code));
			return false;
		}
		// Get some mor data.

		return true;
	}
};

//Process Get Request
static int processGetTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void* stream, long* responseCode,
		write_callback headerCallback, void* headerStream, char* errorBuffer) {
	CURLRaii curl;
	bool ok = curl.init(client, request, callback, stream, headerCallback,
			headerStream, errorBuffer)
			&& curl.setOption(CURLOPT_FOLLOWLOCATION, true)
			&& curl.perform(responseCode);
	return ok ? 0 : 1;
}

//Process POST Request
static int processPostTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void* stream, long* responseCode,
		write_callback headerCallback, void* headerStream, char* errorBuffer) {
	CURLRaii curl;
	bool ok = curl.init(client, request, callback, stream, headerCallback,
			headerStream, errorBuffer) && curl.setOption(CURLOPT_POST, 1)
			&& curl.setOption(CURLOPT_POSTFIELDS, request->getRequestData())
			&& curl.setOption(CURLOPT_POSTFIELDSIZE,
					request->getRequestDataSize())
			&& curl.perform(responseCode);
	return ok ? 0 : 1;
}

//Process PUT Request
static int processPutTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void* stream, long* responseCode,
		write_callback headerCallback, void* headerStream, char* errorBuffer) {
	CURLRaii curl;
	bool ok = curl.init(client, request, callback, stream, headerCallback,
			headerStream, errorBuffer)
			&& curl.setOption(CURLOPT_CUSTOMREQUEST, "PUT")
			&& curl.setOption(CURLOPT_POSTFIELDS, request->getRequestData())
			&& curl.setOption(CURLOPT_POSTFIELDSIZE,
					request->getRequestDataSize())
			&& curl.perform(responseCode);
	return ok ? 0 : 1;
}

//Process DELETE Request
static int processDeleteTask(HttpClient* client, HttpRequest* request,
		write_callback callback, void* stream, long* responseCode,
		write_callback headerCallback, void* headerStream, char* errorBuffer) {
	CURLRaii curl;
	bool ok = curl.init(client, request, callback, stream, headerCallback,
			headerStream, errorBuffer)
			&& curl.setOption(CURLOPT_CUSTOMREQUEST, "DELETE")
			&& curl.setOption(CURLOPT_FOLLOWLOCATION, true)
			&& curl.perform(responseCode);
	return ok ? 0 : 1;
}

// HttpClient implementation
HttpClient* HttpClient::getInstance() {
	if (_httpClient == nullptr) {
//		HNLOG_DEBUG("HttpClient::getInstance");
		_httpClient = new (std::nothrow) HttpClient();
	}

	return _httpClient;
}

void HttpClient::destroyInstance() {
	if (nullptr == _httpClient) {

		return;
	}

//	HNLOG_DEBUG("HttpClient::destroyInstance begin");

	auto thiz = _httpClient;
	_httpClient = nullptr;

// 	thiz->_scheduler->unscheduleAllForTarget(thiz);
// 	thiz->_schedulerMutex.lock();
// 	thiz->_scheduler = nullptr;
// 	thiz->_schedulerMutex.unlock();

	thiz->_requestQueueMutex.lock();
	thiz->_requestQueue.push(thiz->_requestSentinel);
	thiz->_requestQueueMutex.unlock();

	thiz->_sleepCondition.notify_one();
	thiz->decreaseThreadCountAndMayDeleteThis();

//	HNLOG_DEBUG("HttpClient::destroyInstance() finished!");
}

void HttpClient::enableCookies(const char* cookieFile) {
	std::lock_guard<std::mutex> lock(_cookieFileMutex);
	if (cookieFile) {
		_cookieFilename = std::string(cookieFile);
	} else {
		//_cookieFilename = (FileUtils::getInstance()->getRootPath() + "cookie.txt");
		_cookieFilename = "./cookie.txt";
	}
}

void HttpClient::setSSLVerification(const std::string& caFile) {
	std::lock_guard<std::mutex> lock(_sslCaFileMutex);
	_sslCaFilename = caFile;
}

HttpClient::HttpClient() :
		_timeoutForConnect(30), _timeoutForRead(60), _isInited(false), _threadCount(
				0), _requestSentinel(new HttpRequest()), _cookie(nullptr) {
	memset(_responseMessage, 0, RESPONSE_BUFFER_SIZE * sizeof(char));
	//_scheduler = Director::getInstance()->getScheduler();
	increaseThreadCount();
}

HttpClient::~HttpClient() {
	delete _requestSentinel;
	_requestSentinel = NULL;
}

//Lazy create semaphore & mutex & thread
bool HttpClient::lazyInitThreadSemphore() {
	if (_isInited) {
		return true;
	} else {
		auto t = std::thread(&HttpClient::networkThread, this);
		t.detach();
		_isInited = true;
	}

	return true;
}

//Add a get task to queue
void HttpClient::send(HttpRequest* request) {
	if (false == lazyInitThreadSemphore()) {
		return;
	}

	if (!request) {
		return;
	}

	_requestQueueMutex.lock();
	_requestQueue.push(request);
	_requestQueueMutex.unlock();

	// Notify thread start to work
	_sleepCondition.notify_one();
}

void HttpClient::sendImmediate(HttpRequest* request) {
	if (!request) {
		return;
	}

	// Create a HttpResponse object, the default setting is http access failed
	HttpResponse *response = new (std::nothrow) HttpResponse(request);

	auto t = std::thread(&HttpClient::networkThreadAlone, this, request,
			response);
	t.detach();
}

//// Poll and notify main thread if responses exists in queue
//void HttpClient::dispatchResponseCallbacks()
//{
//    // log("HttpClient::dispatchResponseCallbacks is running");
//    //occurs when cocos thread fires but the network thread has already quited
//
//	HttpResponse* response = nullptr;
//	_responseQueueMutex.lock();
//	if (!_responseQueue.empty())
//	{
//		response = _responseQueue.front();
//		_responseQueue.pop();
//	}
//	_responseQueueMutex.unlock();
//
//	if (response)
//	{
//		HttpRequest *request = response->getHttpRequest();
//		const ccHttpRequestCallback& callback = request->getCallback();
//		if (callback != nullptr)
//		{
//			callback(this, response);
//		}
//
//		delete response;
//	}
//}

// Process Response
void HttpClient::processResponse(HttpResponse* response,
		char* responseMessage) {
	auto request = response->getHttpRequest();
	long responseCode = -1;
	int retValue = 0;

	// Process the request -> get response packet
	switch (request->getRequestType()) {
	case HttpRequest::Type::emGET: // HTTP GET
		retValue = processGetTask(this, request, writeData,
				response->getResponseData(), &responseCode, writeHeaderData,
				response->getResponseHeader(), responseMessage);
		break;

	case HttpRequest::Type::emPOST: // HTTP POST
		retValue = processPostTask(this, request, writeData,
				response->getResponseData(), &responseCode, writeHeaderData,
				response->getResponseHeader(), responseMessage);
		break;

	case HttpRequest::Type::emPUT:
		retValue = processPutTask(this, request, writeData,
				response->getResponseData(), &responseCode, writeHeaderData,
				response->getResponseHeader(), responseMessage);
		break;

	case HttpRequest::Type::emDELETE:
		retValue = processDeleteTask(this, request, writeData,
				response->getResponseData(), &responseCode, writeHeaderData,
				response->getResponseHeader(), responseMessage);
		break;

	default:
		//LW_ASSERT(true, "HttpClient: unknown request type, only GET and POSt are supported");
		break;
	}

	// write data to HttpResponse
	response->setResponseCode(responseCode);
	if (retValue != 0) {
		response->setSucceed(false);
		response->setErrorBuffer(responseMessage);
	} else {
		response->setSucceed(true);
	}
}

void HttpClient::increaseThreadCount() {
	_threadCountMutex.lock();
	++_threadCount;
	_threadCountMutex.unlock();
}

void HttpClient::decreaseThreadCountAndMayDeleteThis() {
	bool needDeleteThis = false;
	_threadCountMutex.lock();
	--_threadCount;
	if (0 == _threadCount) {
		needDeleteThis = true;
	}

	_threadCountMutex.unlock();
	if (needDeleteThis) {
		delete this;
	}
}

void HttpClient::setTimeoutForConnect(int value) {
	std::lock_guard<std::mutex> lock(_timeoutForConnectMutex);
	_timeoutForConnect = value;
}

int HttpClient::getTimeoutForConnect() {
	std::lock_guard<std::mutex> lock(_timeoutForConnectMutex);
	return _timeoutForConnect;
}

void HttpClient::setTimeoutForRead(int value) {
	std::lock_guard<std::mutex> lock(_timeoutForReadMutex);
	_timeoutForRead = value;
}

int HttpClient::getTimeoutForRead() {
	std::lock_guard<std::mutex> lock(_timeoutForReadMutex);
	return _timeoutForRead;
}

const std::string& HttpClient::getCookieFilename() {
	std::lock_guard<std::mutex> lock(_cookieFileMutex);
	return _cookieFilename;
}

const std::string& HttpClient::getSSLVerification() {
	std::lock_guard<std::mutex> lock(_sslCaFileMutex);
	return _sslCaFilename;
}

