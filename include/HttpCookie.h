#ifndef __HTTP_COOKIE_H
#define __HTTP_COOKIE_H

#include <string>
#include <vector>
#include "libHttpClient.h"

struct CookiesInfo {
	std::string domain;
	bool tailmatch;
	std::string path;
	bool secure;
	std::string name;
	std::string value;
	std::string expires;
};

class EXPORTS_API HttpCookie {
public:
	void readFile();

	void writeFile();
	void setCookieFileName(const std::string fileName);

	const std::vector<CookiesInfo>* getCookies() const;
	const CookiesInfo* getMatchCookie(const std::string& url) const;
	void updateOrAddCookie(CookiesInfo* cookie);

private:
	std::string _cookieFileName;
	std::vector<CookiesInfo> _cookies;
};

#endif /* __HTTP_COOKIE_H */
