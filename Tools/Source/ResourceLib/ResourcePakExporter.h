#pragma once

#include "Engine/Core/Utility.h"
#include "Engine/Resource/PakDecl.h"
#include <algorithm>
#include <string>
#include <vector>

struct DependencyEntry
{
	std::string fileName;
	std::string usage;
	uint32 fileNameCRC;
	uint32 usageCRC;
	uint32 fileNameCRCNoExt;
};

struct ResourceEntry
{
	std::string srcName;	// There may be multiple files with the same srcName (e.g. model + animations from a single fbx)

	ResourceEntry() {}
	virtual ~ResourceEntry() {}

	virtual const void* GetData() = 0;
	virtual uint32 GetDataSize() = 0;
	// Data which is going to be transfered to the GPU should return false, other data
	// should return true so as to avoid copying memory
	virtual bool KeepDataAfterLoading() { return false; }
	virtual const void* GetCustomHeader() = 0;
	virtual uint32 GetCustomHeaderSize() = 0;

	void SetName(const std::string& fileName, usg::ResourceType eType)
	{
		name = fileName;
		std::string noExt = fileName.substr(0, fileName.find_last_of("."));

		uFileCRC = utl::CRC32(fileName.c_str());
		uFileCRCNoExt = utl::CRC32(noExt.c_str());
		resourceType = eType;
	}
	void AddDependency(const std::string& fileName, const std::string& usage)
	{
		DependencyEntry entry;
		entry.fileName = fileName;
		entry.usage = usage;
		entry.fileNameCRC = utl::CRC32(fileName.c_str());

		std::string noExt = fileName.substr(0, fileName.find_last_of("."));
		entry.fileNameCRCNoExt = utl::CRC32(noExt.c_str());

		entry.usageCRC = utl::CRC32(usage.c_str());
		dependencies.push_back(entry);
	}
	const std::vector<DependencyEntry>& GetDeps() const { return dependencies; }
	const std::string& GetName() const { return name; }
	uint32 GetNameCRC() const { return uFileCRC; }
	uint32 GetNameCRCNoExt() const { return uFileCRCNoExt;  }
	usg::ResourceType GetResourceType() const { return resourceType; }

	bool operator<(const ResourceEntry &rhs) const
	{
		const auto& rhsDeps = rhs.GetDeps();
		if (dependencies.size() == 0 && rhs.dependencies.size() > 0)
			return true;
		for (size_t i = 0; i < rhsDeps.size(); i++)
		{
			if (rhsDeps[i].fileNameCRC == uFileCRC)
			{
				return true;
			}
		}
		return false;
	}


private:
	std::vector<DependencyEntry> dependencies;
	std::string name;
	usg::ResourceType resourceType;
	uint32 uFileCRC;
	uint32 uFileCRCNoExt;
};

namespace ResourcePakExporter
{
	// Orders the entries so that each comes after the entries in this pak it
	// depends on: entries with no dependencies first, then the rest, each
	// group otherwise in the order the entries were added.
	// (This used to be std::sort with ResourceEntry::operator<, which isn't a strict
	// weak ordering, so the result depended on the standard library: MSVC and
	// libstdc++ produced different paks from the same input.)
	inline void SortByDependencies(std::vector<ResourceEntry*>& entries)
	{
		const size_t uCount = entries.size();
		std::vector<std::vector<size_t>> dependents(uCount);
		std::vector<size_t> waitingOn(uCount, 0);
		for (size_t i = 0; i < uCount; i++)
		{
			for (const DependencyEntry& dep : entries[i]->GetDeps())
			{
				for (size_t j = 0; j < uCount; j++)
				{
					if (j != i && entries[j]->GetNameCRC() == dep.fileNameCRC)
					{
						dependents[j].push_back(i);
						waitingOn[i]++;
					}
				}
			}
		}

		std::vector<ResourceEntry*> sorted;
		sorted.reserve(uCount);
		std::vector<bool> placed(uCount, false);
		auto place = [&](size_t i)
		{
			sorted.push_back(entries[i]);
			placed[i] = true;
			for (size_t d : dependents[i])
				waitingOn[d]--;
		};

		for (size_t i = 0; i < uCount; i++)
		{
			if (entries[i]->GetDeps().empty())
				place(i);
		}

		// The lowest-indexed entry whose dependencies are all placed, each time
		bool bProgress = true;
		while (sorted.size() < uCount && bProgress)
		{
			bProgress = false;
			for (size_t i = 0; i < uCount; i++)
			{
				if (!placed[i] && waitingOn[i] == 0)
				{
					place(i);
					bProgress = true;
					break;
				}
			}
		}

		// Circular dependencies: keep the remaining order
		for (size_t i = 0; i < uCount; i++)
		{
			if (!placed[i])
				sorted.push_back(entries[i]);
		}

		entries.swap(sorted);
	}

	// Note entries may be re-ordered by this function
	inline bool Export(const char* szFileName, std::vector<ResourceEntry*>& entries)
	{
		// Sort these entries so that dependencies come before other files
		SortByDependencies(entries);

		uint32 uTmpOffset = sizeof(usg::PakFileDecl::ResourcePakHdr);
		for (uint32 i = 0; i < entries.size(); i++)
		{
			uTmpOffset += sizeof(usg::PakFileDecl::FileInfo);
			uTmpOffset += entries[i]->GetCustomHeaderSize();
			uTmpOffset += (uint32)(entries[i]->GetDeps().size() * sizeof(usg::PakFileDecl::Dependency));
		}

		bool bHasKeepData = false;
		uint32 uKeepOffset = uTmpOffset;
		for (uint32 i = 0; i < entries.size(); i++)
		{
			if (!entries[i]->KeepDataAfterLoading())
			{
				uKeepOffset += entries[i]->GetDataSize();
			}
			else
			{
				bHasKeepData = true;
			}
		}

		if (!bHasKeepData)
		{
			uKeepOffset = USG_INVALID_ID;
		}

		FILE* pFileOut = nullptr;
		fopen_s(&pFileOut, szFileName, "wb");

		if (!pFileOut)
		{
			DEBUG_PRINT("Unable to open file %s", szFileName);
			return false;
		}

		usg::PakFileDecl::ResourcePakHdr hdr;
		hdr.uVersionId = usg::PakFileDecl::CURRENT_VERSION;
		hdr.uFileCount = (uint32)entries.size();
		hdr.uResDataOffset = uKeepOffset;
		hdr.uTempDataOffset = uTmpOffset;

		fwrite(&hdr, sizeof(hdr), 1, pFileOut);

		for (uint32 i = 0; i < entries.size(); i++)
		{
			usg::PakFileDecl::FileInfo fileInfo;
			strcpy_s(fileInfo.szName, entries[i]->GetName().c_str());
			fileInfo.CRC = utl::CRC32(fileInfo.szName);
			fileInfo.CRCNoExt = entries[i]->GetNameCRCNoExt();
			fileInfo.uCustomHeaderSize = entries[i]->GetCustomHeaderSize();
			fileInfo.uDependenciesCount = (uint32)entries[i]->GetDeps().size();
			fileInfo.uDataSize = entries[i]->GetDataSize();
			fileInfo.uResourceType = (uint32)entries[i]->GetResourceType();
			fileInfo.uFileFlags = entries[i]->KeepDataAfterLoading() ? usg::PakFileDecl::FILE_FLAG_KEEP_DATA : 0;
			// TODO: Probably want to have an alignment value for the data
			fileInfo.uTotalFileInfoSize = (uint32)(sizeof(fileInfo) + entries[i]->GetCustomHeaderSize() + (sizeof(usg::PakFileDecl::Dependency) * entries[i]->GetDeps().size()));
			if (fileInfo.uDataSize > 0)
			{
				if (!entries[i]->KeepDataAfterLoading())
				{
					fileInfo.uDataOffset = uTmpOffset;
					uTmpOffset += fileInfo.uDataSize;
				}
				else
				{
					fileInfo.uDataOffset = uKeepOffset;
					uKeepOffset += fileInfo.uDataSize;
				}
			}
			else
			{
				fileInfo.uDataOffset = USG_INVALID_ID;
			}
			fwrite(&fileInfo, sizeof(fileInfo), 1, pFileOut);

			if (fileInfo.uCustomHeaderSize > 0)
			{
				fwrite(entries[i]->GetCustomHeader(), entries[i]->GetCustomHeaderSize(), 1, pFileOut);

			}

			if (fileInfo.uDependenciesCount > 0)
			{
				for (auto& depItr : entries[i]->GetDeps())
				{
					usg::PakFileDecl::Dependency dep;
					dep.FileCRC = depItr.fileNameCRC;
					dep.UsageCRC = depItr.usageCRC;
					dep.PakIndex = USG_INVALID_ID;
					dep.FileCRCNoExt = depItr.fileNameCRCNoExt;
					for (size_t depId = 0; depId < entries.size(); depId++)
					{
						if (entries[depId]->GetNameCRC() == depItr.fileNameCRC)
						{
							dep.PakIndex = (uint32)i;
						}
					}
					fwrite(&dep, sizeof(dep), 1, pFileOut);
				}
			}
		}

		// Now do the GPU data
		for (uint32 i = 0; i < entries.size(); i++)
		{
			if (!entries[i]->KeepDataAfterLoading())
			{
				fwrite(entries[i]->GetData(), entries[i]->GetDataSize(), 1, pFileOut);
			}
		}

		// Now do the CPU data
		for (uint32 i = 0; i < entries.size(); i++)
		{
			if (entries[i]->KeepDataAfterLoading())
			{
				fwrite(entries[i]->GetData(), entries[i]->GetDataSize(), 1, pFileOut);
			}
		}

		fclose(pFileOut);

		return true;
	}
};