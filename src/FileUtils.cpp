#include "FileUtils.h"

#include <queue>
#include <assert.h>
#include <sys/stat.h>
#include <regex>

#define HN_MAX_PATH		260

// The root path of resources, the character encoding is UTF-8.
// UTF-8 is the only encoding supported by cocos2d-x API.
static std::string s_resourcePath = "";

// D:\aaa\bbb\ccc\ddd\abc.txt --> D:/aaa/bbb/ccc/ddd/abc.txt
static inline std::string convertPathFormatToUnixStyle(const std::string& path)
{
	std::string ret = path;
	int len = ret.length();
	for (int i = 0; i < len; ++i)
	{
		if (ret[i] == '\\')
		{
			ret[i] = '/';
		}
	}
	return ret;
}

static int writeData(const std::string& filename, unsigned char* data, unsigned int len, bool forString)
{
	if (filename.empty())
	{
		return -1;
	}

	size_t writesize = 0;
	const char* mode = nullptr;

	if (forString)
		mode = "wt+";
	else
		mode = "wb+";

	do
	{
		// Read the file from hardware
		std::string fullPath = filename;
		FILE *fp = nullptr;
		fp = fopen(fullPath.c_str(), mode);
		if (!fp) break;
		fseek(fp, 0, SEEK_SET);
		writesize = fwrite(data, sizeof(unsigned char), len, fp);
		fclose(fp);

	} while (0);

	return writesize;
}

static Data getData(const std::string& filename, bool forString)
{
	if (filename.empty())
	{
		return Data::Null;
	}

	Data ret;
	unsigned char* buffer = nullptr;
	size_t size = 0;
	size_t readsize;
	const char* mode = nullptr;

	if (forString)
		mode = "rt";
	else
		mode = "rb";

	do
	{
		// Read the file from hardware
		std::string fullPath = filename;
		FILE *fp = nullptr;
		fp = fopen(fullPath.c_str(), mode);
		if (!fp) break;
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		if (forString)
		{
			buffer = (unsigned char*)malloc(sizeof(unsigned char) * (size + 1));
			buffer[size] = '\0';
		}
		else
		{
			buffer = (unsigned char*)malloc(sizeof(unsigned char) * size);
		}

		readsize = fread(buffer, sizeof(unsigned char), size, fp);
		fclose(fp);

		if (forString && readsize < size)
		{
			buffer[readsize] = '\0';
		}
	} while (0);

	if (nullptr == buffer || 0 == readsize)
	{
		std::string msg = "Get data from file(";
		msg.append(filename).append(") failed!");
		printf("%s\n", msg.c_str());
	}
	else
	{
		ret.fastSet(buffer, readsize);
	}

	return ret;
}

FileUtils* s_FileUtils = nullptr;

FileUtils* FileUtils::getInstance()
{
	if (s_FileUtils == nullptr)
	{
		s_FileUtils = new FileUtils();
	}
	return s_FileUtils;
}

//////////////////////////////////////////////////////////////////////////////

FileUtils::FileUtils(void)
{
}

FileUtils::~FileUtils(void)
{
}

std::string FileUtils::getStringFromFile(const std::string& filename)
{
	Data data = getData(filename, true);
	if (data.isNull())
		return "";

	std::string ret((const char*)data.getBytes());
	return ret;
}

Data FileUtils::getDataFromFile(const std::string& filename)
{
	return getData(filename, false);
}

int FileUtils::setStringToFile(const std::string& filename, const std::string& data)
{
	int r = writeData(filename, (unsigned char*)data.c_str(), data.size(), true);
	return r;
}

int FileUtils::setDataToFile(const std::string& filename, unsigned char* data, unsigned int len)
{
	int r = writeData(filename, data, len, false);
	return r;
}


bool FileUtils::isAbsolutePath(const std::string& strPath) const
{
	if (strPath.length() > 2
		&& ((strPath[0] >= 'a' && strPath[0] <= 'z') || (strPath[0] >= 'A' && strPath[0] <= 'Z'))
		&& strPath[1] == ':')
	{
		return true;
	}
	return false;
}

long FileUtils::getFileSize(const std::string &filepath)
{
	assert(!filepath.empty());

	std::string fullpath = filepath;
	if (!isAbsolutePath(filepath))
	{
		return 0;
	}

	struct stat info;
	// Get data associated with "crt_stat.c":
	int result = stat(fullpath.c_str(), &info);

	// Check if statistics are valid:
	if (result != 0)
	{
		// Failed
		return -1;
	}
	else
	{
		return (long)(info.st_size);
	}
}
