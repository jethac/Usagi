#include "Engine/Common/Common.h"
#include "Engine/Resource/PakDecl.h"
#include "Tools/Source/ResourceLib/ResourcePakExporter.h"

#include <algorithm>
#include <cstdio>
#include <direct.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
	struct TestResourceEntry : public ResourceEntry
	{
		explicit TestResourceEntry(const char* szName)
		{
			SetName(szName, usg::ResourceType::CUSTOM_EFFECT);
			data.push_back((uint8)szName[0]);
		}

		const void* GetData() override { return data.data(); }
		uint32 GetDataSize() override { return (uint32)data.size(); }
		const void* GetCustomHeader() override { return nullptr; }
		uint32 GetCustomHeaderSize() override { return 0; }

		std::vector<uint8> data;
	};

	struct PakRecord
	{
		usg::PakFileDecl::FileInfo info;
		std::vector<usg::PakFileDecl::Dependency> dependencies;
	};

	bool Expect(bool condition, const char* szMessage)
	{
		if (!condition)
		{
			printf("FAILED: %s\n", szMessage);
			return false;
		}
		return true;
	}

	std::string JoinPath(const std::string& lhs, const char* rhs)
	{
		if (lhs.empty())
		{
			return rhs;
		}

		const char last = lhs[lhs.size() - 1];
		if (last == '\\' || last == '/')
		{
			return lhs + rhs;
		}

		return lhs + "\\" + rhs;
	}

	bool ReadPakRecords(const char* szPath, usg::PakFileDecl::ResourcePakHdr& hdr, std::vector<PakRecord>& records)
	{
		FILE* pFile = nullptr;
		if (fopen_s(&pFile, szPath, "rb") != 0 || pFile == nullptr)
		{
			printf("FAILED: unable to open %s\n", szPath);
			return false;
		}

		if (fread(&hdr, sizeof(hdr), 1, pFile) != 1)
		{
			fclose(pFile);
			return false;
		}

		records.clear();
		for (uint32 i = 0; i < hdr.uFileCount; i++)
		{
			PakRecord record;
			if (fread(&record.info, sizeof(record.info), 1, pFile) != 1)
			{
				fclose(pFile);
				return false;
			}

			if (record.info.uCustomHeaderSize > 0)
			{
				fseek(pFile, record.info.uCustomHeaderSize, SEEK_CUR);
			}

			record.dependencies.resize(record.info.uDependenciesCount);
			if (!record.dependencies.empty() && fread(record.dependencies.data(), sizeof(usg::PakFileDecl::Dependency), record.dependencies.size(), pFile) != record.dependencies.size())
			{
				fclose(pFile);
				return false;
			}

			records.push_back(record);
		}

		fclose(pFile);
		return true;
	}

	std::vector<uint8> ReadBytes(const std::string& path)
	{
		std::ifstream in(path.c_str(), std::ios::binary);
		return std::vector<uint8>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}

	bool ExportChain(const std::string& outputPath, const std::vector<int>& order)
	{
		TestResourceEntry texture("a_texture.tex");
		TestResourceEntry material("m_material.yml");
		TestResourceEntry model("z_model.mdl");

		material.AddDependency(texture.GetName(), "diffuse");
		model.AddDependency(material.GetName(), "material");

		std::vector<ResourceEntry*> entries;
		ResourceEntry* allEntries[] = { &texture, &material, &model };
		for (int index : order)
		{
			entries.push_back(allEntries[index]);
		}

		return ResourcePakExporter::Export(outputPath.c_str(), entries);
	}

	bool TestDeterministicDependencyOrder(const std::string& outputDir)
	{
		const std::string firstPath = JoinPath(outputDir, "dependency_order_first.pak");
		const std::string secondPath = JoinPath(outputDir, "dependency_order_second.pak");

		if (!Expect(ExportChain(firstPath, { 2, 0, 1 }), "first export succeeds"))
		{
			return false;
		}
		if (!Expect(ExportChain(secondPath, { 1, 2, 0 }), "second export succeeds"))
		{
			return false;
		}

		usg::PakFileDecl::ResourcePakHdr hdr;
		std::vector<PakRecord> records;
		if (!Expect(ReadPakRecords(firstPath.c_str(), hdr, records), "read exported pak records"))
		{
			return false;
		}

		bool ok = true;
		ok &= Expect(hdr.uVersionId == usg::PakFileDecl::CURRENT_VERSION, "pak version matches");
		ok &= Expect(records.size() == 3, "pak contains three files");
		ok &= Expect(strcmp(records[0].info.szName, "a_texture.tex") == 0, "texture sorted first");
		ok &= Expect(strcmp(records[1].info.szName, "m_material.yml") == 0, "material sorted second");
		ok &= Expect(strcmp(records[2].info.szName, "z_model.mdl") == 0, "model sorted third");
		ok &= Expect(records[1].dependencies.size() == 1, "material dependency serialized");
		ok &= Expect(records[1].dependencies[0].PakIndex == 0, "material dependency points at texture");
		ok &= Expect(records[2].dependencies.size() == 1, "model dependency serialized");
		ok &= Expect(records[2].dependencies[0].PakIndex == 1, "model dependency points at material");

		for (uint32 i = 0; i < records.size(); i++)
		{
			for (const auto& dep : records[i].dependencies)
			{
				ok &= Expect(dep.PakIndex < i, "dependency pak index precedes dependent");
			}
		}

		ok &= Expect(ReadBytes(firstPath) == ReadBytes(secondPath), "exports are byte-identical across input order");
		return ok;
	}

	bool TestMissingDependencyFails(const std::string& outputDir)
	{
		TestResourceEntry model("model_with_missing_dep.mdl");
		model.AddDependency("missing_material.yml", "material");

		std::vector<ResourceEntry*> entries;
		entries.push_back(&model);

		const std::string path = JoinPath(outputDir, "missing_dependency.pak");
		return Expect(!ResourcePakExporter::Export(path.c_str(), entries), "missing dependency export fails");
	}

	bool TestCyclicDependencyFails(const std::string& outputDir)
	{
		TestResourceEntry first("cycle_a.bin");
		TestResourceEntry second("cycle_b.bin");
		first.AddDependency(second.GetName(), "next");
		second.AddDependency(first.GetName(), "prev");

		std::vector<ResourceEntry*> entries;
		entries.push_back(&first);
		entries.push_back(&second);

		const std::string path = JoinPath(outputDir, "cyclic_dependency.pak");
		return Expect(!ResourcePakExporter::Export(path.c_str(), entries), "cyclic dependency export fails");
	}
}

int main(int argc, char** argv)
{
	const std::string outputDir = argc > 1 ? argv[1] : "_resource_pak_exporter_tests";
	_mkdir(outputDir.c_str());

	bool ok = true;
	ok &= TestDeterministicDependencyOrder(outputDir);
	ok &= TestMissingDependencyFails(outputDir);
	ok &= TestCyclicDependencyFails(outputDir);

	if (ok)
	{
		printf("ResourcePakExporter tests passed\n");
		return 0;
	}

	return 1;
}
