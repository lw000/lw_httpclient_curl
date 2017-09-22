#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <thread>
#include <condition_variable>
#include <queue>
#include "HttpCookie.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "libHttpClient.h"

class EXPORTS_API HttpClient {
public:
	/**
	 * The buffer size of _responseMessage
	 */
	static const int RESPONSE_BUFFER_SIZE = 256;

	/**
	 * Get instance of HttpClient.
	 *
	 * @return the instance of HttpClient.
	 */
	static HttpClient *getInstance();

	/**
	 * Relase the instance of HttpClient.
	 */
	static void destroyInstance();

	/**
	 * Enable cookie support.
	 *
	 * @param cookieFile the filepath of cookie file.
	 */
	void enableCookies(const char* cookieFile);

	/**
	 * Get the cookie filename
	 *
	 * @return the cookie filename
	 */
	const std::string& getCookieFilename();

	/**
	 * Set root certificate path for SSL verification.
	 *
	 * @param caFile a full path of root certificate.if it is empty, SSL verification is disabled.
	 */
	void setSSLVerification(const std::string& caFile);

	/**
	 * Get ths ssl CA filename
	 *
	 * @return the ssl CA filename
	 */
	const std::string& getSSLVerification();

	/**
	 * Add a get request to task queue
	 *
	 * @param request a HttpRequest object, which includes url, response callback etc.
	 please make sure request->_requestData is clear before calling "send" here.
	 */
	void send(HttpRequest* request);

	/**
	 * Immediate send a request
	 *
	 * @param request a HttpRequest object, which includes url, response callback etc.
	 please make sure request->_requestData is clear before calling "sendImmediate" here.
	 */
	void sendImmediate(HttpRequest* request);

	/**
	 * Set the timeout value for connecting.
	 *
	 * @param value the timeout value for connecting.
	 */
	void setTimeoutForConnect(int value);

	/**
	 * Get the timeout value for connecting.
	 *
	 * @return int the timeout value for connecting.
	 */
	int getTimeoutForConnect();

	/**
	 * Set the timeout value for reading.
	 *
	 * @param value the timeout value for reading.
	 */
	void setTimeoutForRead(int value);

	/**
	 * Get the timeout value for reading.
	 *
	 * @return int the timeout value for reading.
	 */
	int getTimeoutForRead();

	HttpCookie* getCookie() const {
		return _cookie;
	}

	std::mutex& getCookieFileMutex() {
		return _cookieFileMutex;
	}

	std::mutex& getSSLCaFileMutex() {
		return _sslCaFileMutex;
	}

private:
	HttpClient();
	virtual ~HttpClient();
	bool init(void);

	/**
	 * Init pthread mutex, semaphore, and create new thread for http requests
	 * @return bool
	 */
	bool lazyInitThreadSemphore();
	void networkThread();
	void networkThreadAlone(HttpRequest* request, HttpResponse* response);

	/** Poll function called from main thread to dispatch callbacks when http requests finished **/
	//void dispatchResponseCallbacks();
	void processResponse(HttpResponse* response, char* responseMessage);
	void increaseThreadCount();
	void decreaseThreadCountAndMayDeleteThis();

private:
	bool _isInited;

	int _timeoutForConnect;
	std::mutex _timeoutForConnectMutex;

	int _timeoutForRead;
	std::mutex _timeoutForReadMutex;

	int _threadCount;
	std::mutex _threadCountMutex;

	//Scheduler* _scheduler;
	std::mutex _schedulerMutex;

	std::queue<HttpRequest*> _requestQueue;
	std::mutex _requestQueueMutex;

	//std::queue<HttpResponse*> _responseQueue;
	//std::mutex _responseQueueMutex;

	std::string _cookieFilename;
	std::mutex _cookieFileMutex;

	std::string _sslCaFilename;
	std::mutex _sslCaFileMutex;

	HttpCookie* _cookie;

	std::condition_variable_any _sleepCondition;

	char _responseMessage[RESPONSE_BUFFER_SIZE];

	HttpRequest* _requestSentinel;
};

#endif //__HTTP_CLIENT_H__
