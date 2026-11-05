/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Linux file access. Paths are relative to the working
//	directory, as on Windows. Resource names are matched case-insensitively
//	and may use either separator, because cooked data was authored on a
//	case-insensitive file system.
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/File/File.h"
#include "Engine/Core/stl/string.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/Memory/Mem.h"
#include "file_ps.h"
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace usg {

	const char* g_szFileDir[FILE_TYPE_COUNT] =
	{
		"",
		"_savedata/",
		"_dump/",
		"_savedata/"
	};


static const char* s_szAccessStrings[] =
{
	"rb",	// FILE_ACCESS_READ
	"wb",	// FILE_ACCESS_WRITE
	"ab",	// FILE_ACCESS_READ_WRITE_DEPRECATED
};

static bool PathExists(const char* szPath)
{
	struct stat st;
	return stat(szPath, &st) == 0;
}

// Builds the on-disk path for a file: prefixes the directory for its type
// and converts '\\' separators to '/'
static usg::string MakePath(const char* szName, FILE_TYPE eFileType)
{
	usg::string path(g_szFileDir[eFileType]);
	path += szName;
	for (auto& c : path)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	return path;
}

// If szPath doesn't exist as written, look for an existing path that differs
// only in case, one component at a time. Returns false if there is none.
static bool ResolveCase(const usg::string& path, usg::string& resolved)
{
	if (PathExists(path.c_str()))
	{
		resolved = path;
		return true;
	}

	resolved = path.size() > 0 && path[0] == '/' ? "/" : "";
	memsize uStart = path.size() > 0 && path[0] == '/' ? 1 : 0;

	while (uStart < path.size())
	{
		memsize uEnd = path.find('/', uStart);
		if (uEnd == usg::string::npos)
		{
			uEnd = path.size();
		}
		usg::string component = path.substr(uStart, uEnd - uStart);
		uStart = uEnd + 1;

		if (component.empty() || component == ".")
		{
			continue;
		}

		usg::string candidate = resolved + component;
		if (!PathExists(candidate.c_str()))
		{
			DIR* pDir = opendir(resolved.empty() ? "." : resolved.c_str());
			if (!pDir)
			{
				return false;
			}

			bool bFound = false;
			while (struct dirent* pEntry = readdir(pDir))
			{
				if (strcasecmp(pEntry->d_name, component.c_str()) == 0)
				{
					candidate = resolved + pEntry->d_name;
					bFound = true;
					break;
				}
			}
			closedir(pDir);

			if (!bFound)
			{
				return false;
			}
		}

		resolved = candidate;
		if (uEnd < path.size())
		{
			resolved += "/";
		}
	}

	return true;
}

FILE_STATUS File_ps::FileStatus(const char* szName, const FILE_TYPE eFileType)
{
	usg::string resolved;
	if (ResolveCase(MakePath(szName, eFileType), resolved))
	{
		struct stat st;
		if (stat(resolved.c_str(), &st) == 0 && S_ISREG(st.st_mode))
		{
			return FILE_STATUS_VALID;
		}
	}

	return FILE_STATUS_NOT_FOUND;
}

bool File_ps::UserFileOpenPath(const FileOpenPath& pathIn, usg::vector<FilePathResult>& result)
{
	// File dialogs are an editor feature, not yet supported on Linux
	return false;
}

bool File_ps::UserFileSavePath(const FileOpenPath& pathIn, FilePathResult& result)
{
	return false;
}

bool File_ps::Delete(const char* szName, FILE_TYPE eFileType)
{
	usg::string resolved;
	if (!ResolveCase(MakePath(szName, eFileType), resolved))
	{
		return false;
	}
	return ( remove(resolved.c_str()) == 0);
}


bool File_ps::CreateFileDirectory(const char* szName, FILE_TYPE eType)
{
	usg::string path = MakePath(szName, eType);

	return mkdir(path.c_str(), 0755) == 0;
}

size_t File_ps::NumberOfFilesInDirectory(const char* szDirName, FILE_TYPE eFileType,
                                         size_t maximumFileCount)
{
	usg::string resolved;
	if (!ResolveCase(MakePath(szDirName, eFileType), resolved))
	{
		return 0;
	}

	DIR* pDir = opendir(resolved.c_str());
	ASSERT(pDir != nullptr);
	if (!pDir)
	{
		return 0;
	}

	size_t entryCount = 0;
	while (struct dirent* pEntry = readdir(pDir))
	{
		usg::string entryPath = resolved + "/" + pEntry->d_name;
		struct stat st;
		if (stat(entryPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
		{
			entryCount++;
		}
	}
	closedir(pDir);

	return entryCount;
}

FILE_INIT_RESULT File_ps::InitFileSystem()
{
	mkdir("_savedata", 0755);
	mkdir("_dump", 0755);

	return FILE_INIT_RESULT_SUCCESS;
}

File_ps::File_ps()
	: m_pFile(nullptr)
{
}

File_ps::File_ps(const char* szFileName, FILE_ACCESS_MODE eMode, FILE_TYPE eFileType)
{
	m_pFile = NULL;
	Open(szFileName, eMode, eFileType);
}

File_ps::~File_ps()
{
	Close();
}


bool File_ps::Open(const char* szFileName, FILE_ACCESS_MODE eMode, FILE_TYPE eFileType)
{
	usg::string path = MakePath(szFileName, eFileType);
	usg::string resolved;
	if (eMode == FILE_ACCESS_READ)
	{
		if (!ResolveCase(path, resolved))
		{
			return false;
		}
	}
	else
	{
		// Writing: keep an existing file's case, otherwise create it as named
		if (!ResolveCase(path, resolved))
		{
			resolved = path;
		}
	}

	m_pFile = fopen(resolved.c_str(), s_szAccessStrings[eMode]);

	return (m_pFile!=NULL );
}

void File_ps::Close()
{
	if(m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
}

bool File_ps::IsOpen()
{
	return (m_pFile!=NULL);
}

memsize File_ps::Read(memsize uSize, void* pDst)
{
	size_t uSizeRead = fread(pDst, 1, uSize, m_pFile);

	return uSizeRead;
}

memsize File_ps::Write(memsize uSize, const void* pSrc)
{
	size_t uSizeWritten = fwrite(pSrc, 1, uSize, m_pFile);

	ASSERT(uSizeWritten == uSize);

	return uSizeWritten;
}

uint8 File_ps::ReadByte()
{
	return fgetc(m_pFile);
}

void File_ps::AdvanceBytes(memsize uAdvance)
{
	fseeko(m_pFile, (off_t)uAdvance, SEEK_CUR);
}

void File_ps::SeekPos(memsize uPos)
{
	fseeko(m_pFile, (off_t)uPos, SEEK_SET);
}

void File_ps::SeekEnd()
{
	ASSERT(m_pFile!=NULL);

	fseeko(m_pFile, 0, SEEK_END);
}

memsize File_ps::GetPos()
{
	ASSERT(m_pFile!=NULL);

	return (memsize)ftello(m_pFile);
}

memsize File_ps::GetSize()
{
	memsize uSize;
	memsize uPrevPos = GetPos();

	SeekEnd();
	uSize = GetPos();

	SeekPos(uPrevPos);

	return uSize;
}

}
