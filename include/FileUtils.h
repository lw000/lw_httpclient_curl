#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

#include <string>
#include "Data.h"

class FileUtils final
{
public:
	static FileUtils* getInstance();

public:
	std::string getStringFromFile(const std::string& filename);
	Data getDataFromFile(const std::string& filename);

	int setStringToFile(const std::string& filename, const std::string& data);
	int setDataToFile(const std::string& filename, unsigned char* data, unsigned int len);

public:
	bool isAbsolutePath(const std::string& strPath) const;
	long getFileSize(const std::string &filepath);

protected:
	FileUtils(void);
	~FileUtils(void);
};

#endif // __FILE_UTILS_H__
