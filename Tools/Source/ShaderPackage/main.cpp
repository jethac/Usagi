#include "Engine/Common/Common.h"
#include "Engine/Graphics/RenderConsts.h"
#include "Engine/Graphics/Textures/TGAFile.h"
#include "gli/gli.hpp"
#include "Engine/Core/ProtocolBuffers/ProtocolBufferFile.h"
#include "Engine/Resource/PakDecl.h"
#include "Engine/Core/Utility.h"
#include "../ResourceLib/MaterialDefinition/MaterialDefinitionExporter.h"
#include "Engine/Layout/Fonts/TextStructs.pb.h"
#include "ResourcePakExporter.h"
#include <yaml-cpp/yaml.h>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "VulkanShaderCompiler.h"
#include "OpenGLShaderCompiler.h"
#include <pb.h>




const char* g_szExtensions[] =
{
	".vert",
	".frag",
	".geom",
	".tesc",
	".tese"
};

const char* g_szUsageStrings[] =
{
	"vertex_shader",
	"fragment_shader",
	"geometry_shader",
	"tessellation_control_shader",
	"tessellation_evaluation_shader"
};


	
struct EffectEntry : public ResourceEntry
{
	virtual const void* GetData() override { return nullptr; }
	virtual uint32 GetDataSize() override { return 0; };
	virtual const void* GetCustomHeader() { return nullptr; }
	virtual uint32 GetCustomHeaderSize() { return 0; }

};



struct CustomFXEntry : public ResourceEntry
{
	virtual const void* GetData() override { return materialDef.GetBinary(); }
	virtual uint32 GetDataSize() override { return materialDef.GetBinarySize(); };
	virtual const void* GetCustomHeader() { return &materialDef.GetHeader(); }
	virtual uint32 GetCustomHeaderSize() { return materialDef.GetHeaderSize();  }
	// We keep the data as it is, just fix up the pointers
	virtual bool KeepDataAfterLoading() { return true; }

	MaterialDefinitionExporter	materialDef;
	uint64						definitionCRC;	// Rather than getting overly clever we just check for duplicates

};


struct DefineSets
{
	std::string name;
	std::string defines;
	std::string defineSetName;
	std::string definesAsCRC;
	std::string customFXName;
	uint32		CRC[(uint32)usg::ShaderType::COUNT];
	uint64		CustomFXCRC;
};

struct EffectDefinition
{
	std::string name;
	std::string customFXName;
	std::string prog[(uint32)usg::ShaderType::COUNT];

	std::vector<DefineSets> sets;

};





bool CheckArgument(std::string& target, const std::string& argument)
{
	if (strncmp(target.c_str(), argument.c_str(), argument.length()) == 0)
	{
		target.erase(0, argument.length());
		return true;
	}
	else
	{
		return false;
	}
}

enum ArgumentReadResult
{
	ARGUMENT_NO_MATCH,
	ARGUMENT_MATCH,
	ARGUMENT_ERROR
};

ArgumentReadResult ReadArgumentValue(std::string& value, std::string& arg, const char* option, int& index, int argc, char *argv[])
{
	if (arg == option)
	{
		if (index + 1 >= argc)
		{
			fprintf(stderr, "Missing value for %s\n", option);
			return ARGUMENT_ERROR;
		}

		value = argv[++index];
		if (value.empty() || value.at(0) == '-')
		{
			fprintf(stderr, "Missing value for %s\n", option);
			return ARGUMENT_ERROR;
		}

		return ARGUMENT_MATCH;
	}

	if (CheckArgument(arg, option))
	{
		if (arg.empty())
		{
			fprintf(stderr, "Missing value for %s\n", option);
			return ARGUMENT_ERROR;
		}

		value = arg;
		return ARGUMENT_MATCH;
	}

	return ARGUMENT_NO_MATCH;
}

void PrintYamlError(const std::string& fileName, const std::string& fieldPath, const std::string& message, const YAML::Node& node)
{
	if (node)
	{
		YAML::Mark mark = node.Mark();
		if (mark.line >= 0)
		{
			fprintf(stderr, "%s:%d:%d: %s: %s\n", fileName.c_str(), mark.line + 1, mark.column + 1, fieldPath.c_str(), message.c_str());
			return;
		}
	}

	fprintf(stderr, "%s: %s: %s\n", fileName.c_str(), fieldPath.c_str(), message.c_str());
}

bool RequireMap(const std::string& fileName, const std::string& fieldPath, const YAML::Node& node)
{
	if (!node || !node.IsMap())
	{
		PrintYamlError(fileName, fieldPath, "expected a map", node);
		return false;
	}
	return true;
}

bool RequireSequence(const std::string& fileName, const std::string& fieldPath, const YAML::Node& node)
{
	if (!node || !node.IsSequence())
	{
		PrintYamlError(fileName, fieldPath, "expected a sequence", node);
		return false;
	}
	return true;
}

bool ReadRequiredString(const std::string& fileName, const std::string& fieldPath, const YAML::Node& parent, const char* fieldName, std::string& value)
{
	YAML::Node node = parent[fieldName];
	if (!node || !node.IsScalar())
	{
		PrintYamlError(fileName, fieldPath + "." + fieldName, "expected a scalar string", node);
		return false;
	}

	value = node.Scalar();
	if (value.empty())
	{
		PrintYamlError(fileName, fieldPath + "." + fieldName, "value must not be empty", node);
		return false;
	}

	return true;
}

bool ReadOptionalString(const std::string& fileName, const std::string& fieldPath, const YAML::Node& parent, const char* fieldName, std::string& value)
{
	YAML::Node node = parent[fieldName];
	if (!node)
	{
		return true;
	}
	if (!node.IsScalar())
	{
		PrintYamlError(fileName, fieldPath + "." + fieldName, "expected a scalar string", node);
		return false;
	}

	value = node.Scalar();
	return true;
}

bool ReadOptionalBool(const std::string& fileName, const std::string& fieldPath, const YAML::Node& parent, const char* fieldName, bool& value)
{
	YAML::Node node = parent[fieldName];
	if (!node)
	{
		return true;
	}
	if (!node.IsScalar())
	{
		PrintYamlError(fileName, fieldPath + "." + fieldName, "expected true or false", node);
		return false;
	}

	std::string scalar = node.Scalar();
	if (scalar == "true" || scalar == "True" || scalar == "TRUE" || scalar == "1")
	{
		value = true;
		return true;
	}
	if (scalar == "false" || scalar == "False" || scalar == "FALSE" || scalar == "0")
	{
		value = false;
		return true;
	}

	PrintYamlError(fileName, fieldPath + "." + fieldName, "expected true or false", node);
	return false;
}

IShaderCompiler* pCompiler = nullptr;

int main(int argc, char *argv[])
{
	SetEnvironmentVariableA("USAGI_FATAL_STDERR", "1");

	std::string inputFile;
	std::string outBinary;
	std::string shaderDir;
	std::string tempDir;
	std::string dependencyFile;
	std::string api;
	std::string intFileName;
	std::string packageName;
	std::string includeDirs;
	std::string arg;
	ArgumentReadResult readResult;

	for (int i = 1; i < argc; i++)
	{
		arg = argv[i];
		if (arg.empty())
		{
			continue;
		}

		if (arg.at(0) != '-')
		{
			if (!inputFile.empty())
			{
				fprintf(stderr, "Unexpected positional argument '%s' after input file '%s'\n", arg.c_str(), inputFile.c_str());
				return -1;
			}
			inputFile = arg;
		}
		else if ((readResult = ReadArgumentValue(api, arg, "-a", i, argc, argv)) != ARGUMENT_NO_MATCH)
		{
			if (readResult == ARGUMENT_ERROR) return -1;
		}
		else if ((readResult = ReadArgumentValue(outBinary, arg, "-o", i, argc, argv)) != ARGUMENT_NO_MATCH)
		{
			if (readResult == ARGUMENT_ERROR) return -1;
		}
		else if ((readResult = ReadArgumentValue(tempDir, arg, "-t", i, argc, argv)) != ARGUMENT_NO_MATCH)
		{
			if (readResult == ARGUMENT_ERROR) return -1;
		}
		else if ((readResult = ReadArgumentValue(shaderDir, arg, "-s", i, argc, argv)) != ARGUMENT_NO_MATCH)
		{
			if (readResult == ARGUMENT_ERROR) return -1;
		}
		else if (arg == "-i")
		{
			if (i + 1 >= argc || argv[i + 1][0] == '\0' || argv[i + 1][0] == '-')
			{
				fprintf(stderr, "Missing value for -i\n");
				return -1;
			}

			if (!includeDirs.empty())
				includeDirs += " ";
			includeDirs += "-I" + std::string(argv[++i]);
		}
		else if (CheckArgument(arg, "-i"))
		{
			if (arg.empty())
			{
				fprintf(stderr, "Missing value for -i\n");
				return -1;
			}

			if (!includeDirs.empty())
				includeDirs += " ";
			includeDirs += "-I" + arg;
		}
		else
		{
			fprintf(stderr, "Unknown argument '%s'\n", arg.c_str());
			return -1;
		}
	}

	if (inputFile.empty())
	{
		fprintf(stderr, "No shader package input file specified\n");
		return -1;
	}
	if (outBinary.empty())
	{
		fprintf(stderr, "No output package specified; use -o<file> or -o <file>\n");
		return -1;
	}
	if (tempDir.empty())
	{
		fprintf(stderr, "No temporary shader output directory specified; use -t<dir> or -t <dir>\n");
		return -1;
	}
	if (shaderDir.empty())
	{
		fprintf(stderr, "No shader source directory specified; use -s<dir> or -s <dir>\n");
		return -1;
	}

	if (api == "vulkan")
	{
		pCompiler = new VulkanShaderCompiler;
	}
	else if (api == "ogl")
	{
		pCompiler = new OpenGLShaderCompiler;
	}
	else
	{
		printf("Invalid API");
		return -1;
	}
	pCompiler->Init();

	//dependencyFile = argv[6];
	dependencyFile = outBinary + ".d";
	
	intFileName = inputFile.substr(inputFile.find_last_of("\\/") + 1, inputFile.size());
	intFileName = intFileName.substr(0, intFileName.find_last_of("."));

	printf("Converting %s\n", inputFile.c_str());
	fflush(stdout);

	std::ifstream inputFileCheck(inputFile.c_str());
	if (!inputFileCheck.good())
	{
		fprintf(stderr, "Failed to load shader package '%s': file does not exist or cannot be opened\n", inputFile.c_str());
		return -1;
	}
	inputFileCheck.close();

	YAML::Node mainNode = YAML::LoadFile(inputFile.c_str());
	YAML::Node yamlEffect = mainNode["Effects"];
	YAML::Node customFX = mainNode["CustomEffects"];
	if (!RequireMap(inputFile, "<root>", mainNode))
	{
		return -1;
	}
	if (!RequireSequence(inputFile, "Effects", yamlEffect))
	{
		return -1;
	}
	if (customFX && !RequireMap(inputFile, "CustomEffects", customFX))
	{
		return -1;
	}

	std::map<uint32, ShaderEntry> requiredShaders[(uint32)usg::ShaderType::COUNT];
	std::vector<EffectDefinition> effects;
	std::vector<std::string> referencedFiles;
	std::stringstream effectDependencies;

	{
		std::string formatted = inputFile;
		std::replace(formatted.begin(), formatted.end(), '\\', '/');
		effectDependencies << formatted << ": ";
	}

	std::vector<CustomFXEntry> customFXEntries;
	
	uint32 effectIndex = 0;
	for (YAML::const_iterator it = yamlEffect.begin(); it != yamlEffect.end(); ++it)
	{
		YAML::Node effectNode = *it;
		std::string effectPath = std::string("Effects[") + std::to_string(effectIndex) + "]";
		if (!RequireMap(inputFile, effectPath, effectNode))
		{
			return -1;
		}

		EffectDefinition def;
		if (!ReadRequiredString(inputFile, effectPath, effectNode, "name", def.name)
			|| !ReadRequiredString(inputFile, effectPath, effectNode, "vert", def.prog[(uint32)usg::ShaderType::VS])
			|| !ReadRequiredString(inputFile, effectPath, effectNode, "frag", def.prog[(uint32)usg::ShaderType::PS])
			|| !ReadOptionalString(inputFile, effectPath, effectNode, "custom_effect", def.customFXName))
		{
			return -1;
		}
		if (!def.customFXName.empty() && (!customFX || !customFX[def.customFXName]))
		{
			PrintYamlError(inputFile, effectPath + ".custom_effect", std::string("references missing CustomEffects entry '") + def.customFXName + "'", effectNode["custom_effect"]);
			return -1;
		}

		{
			bool bHasDefault = true;
			if (!ReadOptionalBool(inputFile, effectPath, effectNode, "has_default", bHasDefault))
			{
				return -1;
			}

			DefineSets set;
			if (bHasDefault)
			{
				set.name = intFileName + "." + def.name + ".fx";
				set.defineSetName = "";
				set.definesAsCRC = "";
				set.defines = "";
				set.customFXName = def.customFXName;
				if (!ReadOptionalString(inputFile, effectPath, effectNode, "geom", def.prog[(uint32)usg::ShaderType::GS])
					|| !ReadOptionalString(inputFile, effectPath, effectNode, "tesc", def.prog[(uint32)usg::ShaderType::TC])
					|| !ReadOptionalString(inputFile, effectPath, effectNode, "tese", def.prog[(uint32)usg::ShaderType::TE]))
				{
					return -1;
				}
				def.sets.push_back(set);
			}

			if (effectNode["define_sets"])
			{
				YAML::Node defineSets = effectNode["define_sets"];
				if (!RequireSequence(inputFile, effectPath + ".define_sets", defineSets))
				{
					return -1;
				}
				uint32 defineSetIndex = 0;
				for (YAML::const_iterator defineIt = defineSets.begin(); defineIt != defineSets.end(); ++defineIt)
				{
					YAML::Node defineNode = *defineIt;
					std::string definePath = effectPath + ".define_sets[" + std::to_string(defineSetIndex) + "]";
					if (!RequireMap(inputFile, definePath, defineNode))
					{
						return -1;
					}

					// Package.Effect.DefineSet.fx
					std::string defineName;
					if (!ReadRequiredString(inputFile, definePath, defineNode, "name", defineName)
						|| !ReadRequiredString(inputFile, definePath, defineNode, "defines", set.defines))
					{
						return -1;
					}
					set.defineSetName = std::string(".") + defineName;
					set.customFXName = def.customFXName;
					set.name = intFileName + "." + def.name + set.defineSetName + ".fx";
					set.definesAsCRC = std::string(".") + std::to_string(utl::CRC32(set.defines.c_str()));
					def.sets.push_back(set);
					defineSetIndex++;
				}
			}

			uint32 uStandardSets = (uint32)def.sets.size();

			// Anything in a "global" define set is a combination of defines appended to every other shader
			if (effectNode["global_sets"])
			{
				YAML::Node globalSets = effectNode["global_sets"];
				if (!RequireSequence(inputFile, effectPath + ".global_sets", globalSets))
				{
					return -1;
				}
				uint32 globalSetIndex = 0;
				for (YAML::const_iterator globalIt = globalSets.begin(); globalIt != globalSets.end(); ++globalIt)
				{
					YAML::Node globalNode = *globalIt;
					std::string globalPath = effectPath + ".global_sets[" + std::to_string(globalSetIndex) + "]";
					if (!RequireMap(inputFile, globalPath, globalNode))
					{
						return -1;
					}

					// Package.Effect.DefineSet.fx
					std::string name;
					std::string defines;
					if (!ReadRequiredString(inputFile, globalPath, globalNode, "name", name)
						|| !ReadRequiredString(inputFile, globalPath, globalNode, "defines", defines))
					{
						return -1;
					}

					for (uint32 i = 0; i < uStandardSets; i++)
					{
						set = def.sets[i];
						if (set.defines.length() > 0)
						{
							set.defines = set.defines + " " + defines;
						}
						else
						{
							set.defines = defines;
						}
						set.definesAsCRC = std::string(".") + std::to_string(utl::CRC32(set.defines.c_str()));
						set.defineSetName = set.defineSetName + std::string(".") + name;
						set.name = intFileName + "." + def.name + set.defineSetName + ".fx";
						def.sets.push_back(set);
					}
					globalSetIndex++;
				}
			}
		}

		for (uint32 i = 0; i < def.sets.size(); i++)
		{
			def.sets[i].CustomFXCRC = 0;
			MaterialDefinitionExporter* pDef = nullptr;
			if (def.customFXName.size() > 0)
			{
				CustomFXEntry entry;
				entry.materialDef.Load(customFX[def.customFXName], def.sets[i].defines);
				entry.materialDef.InitBinaryData();
				entry.materialDef.InitAutomatedCode();
				uint64 uCustomFXCRC = entry.materialDef.GetCRC();
				bool bFound = false;
				for (int i = 0; i < customFXEntries.size(); i++)
				{
					if (customFXEntries[i].definitionCRC == uCustomFXCRC)
					{
						pDef = &customFXEntries[i].materialDef;
						bFound = true;
					}
				}
				if (!bFound)
				{
					std::string customFXName = intFileName + "." + def.customFXName + "." + std::to_string(customFXEntries.size()) + ".cfx";
					entry.SetName(customFXName, usg::ResourceType::CUSTOM_EFFECT);
					entry.definitionCRC = uCustomFXCRC;
					customFXEntries.push_back(entry);
					pDef = &customFXEntries.back().materialDef;
				}
				def.sets[i].CustomFXCRC = uCustomFXCRC;
			}
			for (uint32 j = 0; j < (uint32)usg::ShaderType::COUNT; j++)
			{
				std::string progName = intFileName + "." + def.prog[j] + def.sets[i].customFXName + def.sets[i].definesAsCRC + g_szExtensions[j] + ".SPV";
				if (!def.prog[j].empty())
				{
					def.sets[i].CRC[j] = utl::CRC32(progName.c_str());
				}
				else
				{
					def.sets[i].CRC[j] = 0;
				}

				if (def.sets[i].CRC[j] != 0)
				{
					if (requiredShaders[j].find(def.sets[i].CRC[j]) == requiredShaders[j].end())
					{
						std::string inputFileName = def.prog[j] + g_szExtensions[j];
						inputFileName = shaderDir + "/" + inputFileName;
						ShaderEntry shader;
						shader.SetName(progName, usg::ResourceType::SHADER);
						shader.entry.eShaderType = (usg::ShaderType)(j);
						bool bSuccess = false;
						std::string tempFileName = intFileName + ".SPV";
						tempFileName = tempDir + "/" + tempFileName;
						bSuccess = pCompiler->Compile(inputFileName, def.sets[i].defines, tempFileName, includeDirs, shader, pDef, referencedFiles, (usg::ShaderType)j);
						if (!bSuccess)
						{
							return -1;
						}
						requiredShaders[j][def.sets[i].CRC[j]] = shader;
					}
				}
			}
		}
		effects.push_back(def);
		effectIndex++;
	}

	for (uint32 i = 0; i < referencedFiles.size(); i++)
	{
		effectDependencies << referencedFiles[i] << " ";
	}

	std::vector<ResourceEntry*> resources;
	std::vector<EffectEntry> effectEntries;

	for (auto& effectItr : effects)
	{
		for (auto& setItr : effectItr.sets)
		{
			EffectEntry effect;
			effect.SetName(setItr.name, usg::ResourceType::EFFECT);

			for (uint32 i = 0; i < (uint32)usg::ShaderType::COUNT; i++)
			{
				if (setItr.CRC[i] != 0)
				{
					auto shaderEntry = requiredShaders[i].find(setItr.CRC[i]);
					if (shaderEntry != requiredShaders[i].end())
					{
						effect.AddDependency((*shaderEntry).second.GetName(), g_szUsageStrings[i]);
					}
				}
			}

			if (setItr.CustomFXCRC != 0)
			{
				for (auto& customFX : customFXEntries)
				{
					if (customFX.definitionCRC == setItr.CustomFXCRC)
					{
						effect.AddDependency(customFX.GetName(), "CustomFX");
						break;
					}
				}
			}
			effectEntries.push_back(effect);
		} 
	} 

	for (uint32 i = 0; i < (uint32)usg::ShaderType::COUNT; i++)
	{
		for (auto& itr : requiredShaders[i])
		{
			ResourceEntry* entry = &itr.second;
			resources.push_back(entry);
		}
	}

	for (auto& effectItr : effectEntries)
	{
		resources.push_back(&effectItr);
	}

	for (auto& customFX : customFXEntries)
	{
		resources.push_back(&customFX);
	}

	// Write out the file
	bool bExportSuccess = ResourcePakExporter::Export(outBinary.c_str(), resources);

	// Delete the binary data
	for (uint32 i = 0; i < (uint32)usg::ShaderType::COUNT; i++)
	{
		for (auto& itr : requiredShaders[i])
		{
			if (itr.second.binary)
			{
				delete itr.second.binary;
				itr.second.binary = nullptr;
			}

		}
	}

	// Spit out the dependencies
	std::ofstream depFile(dependencyFile.c_str(), std::ofstream::binary);
	depFile.clear();
	depFile << effectDependencies.str();

	pCompiler->Cleanup();
	delete pCompiler;

	return bExportSuccess ? 0 : -1;
}
