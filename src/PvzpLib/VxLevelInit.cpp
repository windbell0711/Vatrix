// vx: world-6 level init from Properties/adventure_info.csv, ported from the removed
// src/python/vx_init_lvl.py. Runtime Python is used only for player scripts; this is
// pure C++ data parsing. Shuffles use std::mt19937, so layouts differ from the old
// Python build (accepted one-time change).

#include "VxLevelInit.h"

#include "ConstEnums.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace VX
{
	namespace
	{
		// vx: CSV alias -> code table (mirrors the removed vx_init_lvl._ALIAS2CODE)
		constexpr std::pair<const char*, int> gAlias2Code[] = {
			{"pea", 0}, {"sun", 1}, {"che", 2}, {"nut", 3}, {"min", 4}, {"sno", 5}, {"cho", 6}, {"rep", 7}, {"puf", 8},
			{"ssh", 9}, {"fum", 10}, {"gra", 11}, {"hyp", 12}, {"sca", 13}, {"ice", 14}, {"doo", 15}, {"lil", 16},
			{"squ", 17}, {"thr", 18}, {"kel", 19}, {"jal", 20}, {"spi", 21}, {"tor", 22}, {"tll", 23}, {"sea", 24},
			{"lan", 25}, {"cac", 26}, {"blo", 27}, {"spl", 28}, {"sta", 29}, {"pum", 30}, {"mag", 31}, {"cab", 32},
			{"pot", 33}, {"ker", 34}, {"cof", 35}, {"gar", 36}, {"umb", 37}, {"mar", 38}, {"mel", 39}, {"gat", 40},
			{"twi", 41}, {"glo", 42}, {"cat", 43}, {"win", 44}, {"gol", 45}, {"spr", 46}, {"pao", 47}, {"exn", 49},
			{"gin", 50}, {"rre", 52}, {"zom", 100}, {"flg", 101}, {"con", 102}, {"pol", 103}, {"bkt", 104},
			{"pap", 105}, {"dor", 106}, {"ftb", 107}, {"dan", 108}, {"dab", 109}, {"duk", 110}, {"snk", 111},
			{"zbn", 112}, {"zbt", 113}, {"dol", 114}, {"jac", 115}, {"bal", 116}, {"dig", 117}, {"pog", 118},
			{"yet", 119}, {"bun", 120}, {"lad", 121}, {"ctp", 122}, {"ggt", 123}, {"imp", 124}, {"bos", 125},
			{"pez", 126}, {"nuz", 127}, {"jaz", 128}, {"gaz", 129}, {"sqz", 130}, {"taz", 131}, {"red", 132},
			{"ept", -1}, {"empty", -1}, {"sun25", -25}, {"sun50", -50}, {"sun75", -75},
		};

		int VxAliasToCode(const std::string& theAlias)
		{
			for (const auto& aPair : gAlias2Code)
			{
				if (theAlias == aPair.first)
					return aPair.second;
			}
			return -2;
		}

		std::vector<std::string> VxSplit(const std::string& theText, char theSep)
		{
			std::vector<std::string> aParts;
			std::string aCur;
			for (char aChar : theText)
			{
				if (aChar == theSep)
				{
					aParts.push_back(aCur);
					aCur.clear();
				}
				else
				{
					aCur += aChar;
				}
			}
			aParts.push_back(aCur);
			return aParts;
		}

		std::string VxTrim(const std::string& theText)
		{
			size_t aBegin = 0;
			while (aBegin < theText.size() && (theText[aBegin] == ' ' || theText[aBegin] == '\t' || theText[aBegin] == '\r'))
				aBegin++;
			size_t anEnd = theText.size();
			while (anEnd > aBegin && (theText[anEnd - 1] == ' ' || theText[anEnd - 1] == '\t' || theText[anEnd - 1] == '\r'))
				anEnd--;
			return theText.substr(aBegin, anEnd - aBegin);
		}

		bool VxParseInt(const std::string& theText, int& theOut)
		{
			std::string aTrimmed = VxTrim(theText);
			if (aTrimmed.empty())
				return false;
			char* anEnd = nullptr;
			long aValue = std::strtol(aTrimmed.c_str(), &anEnd, 10);
			if (anEnd != aTrimmed.c_str() + aTrimmed.size())
				return false;
			theOut = static_cast<int>(aValue);
			return true;
		}

		// vx: one vase-content pool (alias, style) per category, insertion order preserved
		struct VxPotPool
		{
			std::string mCategory;
			std::vector<std::pair<std::string, std::string>> mItems;
		};

		// vx: one scene rule: cell key -> names for cells with that key, plus its pool
		struct VxSceneRule
		{
			std::string mKey;
			std::vector<std::string> mRule;
			std::vector<std::string> mPool;
		};

		bool VxLoadCsv(const std::filesystem::path& theCsvPath, std::vector<std::vector<std::string>>& theRows)
		{
			std::ifstream aStream(theCsvPath, std::ios::binary);
			if (!aStream)
				return false;
			std::string aData((std::istreambuf_iterator<char>(aStream)), std::istreambuf_iterator<char>());
			if (aData.size() >= 3 && static_cast<unsigned char>(aData[0]) == 0xEF &&
				static_cast<unsigned char>(aData[1]) == 0xBB && static_cast<unsigned char>(aData[2]) == 0xBF)
				aData.erase(0, 3);
			theRows.clear();
			for (const std::string& aLine : VxSplit(aData, '\n'))
			{
				std::string aRowText = aLine;
				if (!aRowText.empty() && aRowText.back() == '\r')
					aRowText.pop_back();
				if (aRowText.empty())
					continue;
				std::vector<std::string> aFields;
				std::string aCur;
				for (char aChar : aRowText)
				{
					if (aChar == ',')
					{
						aFields.push_back(aCur);
						aCur.clear();
					}
					else
					{
						aCur += aChar;
					}
				}
				aFields.push_back(aCur);
				theRows.push_back(aFields);
			}
			return true;
		}

		bool VxParseSceneRules(const VxLevelInfo& theInfo, std::vector<std::vector<std::vector<std::string>>>& theArray,
			std::vector<VxSceneRule>& thePools)
		{
			if (theInfo.mSceneRuleCode != 2)
				return false;
			std::vector<VxSceneRule> aRules;
			for (const std::string& aPart : VxSplit(theInfo.mSceneRule, ';'))
			{
				if (aPart.size() < 2 || aPart[1] != ':')
					continue;
				VxSceneRule aRule;
				aRule.mKey = aPart.substr(0, 1);
				std::string aValue = aPart.substr(2);
				if (aValue.find('*') != std::string::npos)
				{
					for (const std::string& aItem : VxSplit(aValue, '+'))
					{
						size_t aStar = aItem.find('*');
						if (aStar == std::string::npos)
						{
							aRule.mPool.push_back(aItem);
							continue;
						}
						int aCount = 0;
						VxParseInt(aItem.substr(aStar + 1), aCount);
						for (int i = 0; i < aCount; i++)
							aRule.mPool.push_back(aItem.substr(0, aStar));
					}
					aRule.mRule.push_back(aRule.mKey);
				}
				else
				{
					aRule.mRule = VxSplit(aValue, '&');
					aRule.mPool = aRule.mRule;
				}
				aRules.push_back(aRule);
			}
			theArray.clear();
			for (const std::string& aDesignRow : VxSplit(theInfo.mSceneDesign, '/'))
			{
				std::vector<std::vector<std::string>> aRow;
				for (char aCell : aDesignRow)
				{
					std::vector<std::string> aCellNames;
					for (const VxSceneRule& aRule : aRules)
					{
						if (aRule.mKey == std::string(1, aCell))
							aCellNames = aRule.mRule;
					}
					aRow.push_back(aCellNames);
				}
				theArray.push_back(aRow);
			}
			thePools = aRules;
			return !theArray.empty();
		}

		bool VxParseVaseRules(const VxLevelInfo& theInfo, std::vector<VxPotPool>& theOut)
		{
			theOut.clear();
			int aCode = theInfo.mVaseRuleCode;
			if (aCode == 1)
			{
				VxPotPool aPool;
				aPool.mCategory = "1";
				for (const std::string& aItem : VxSplit(theInfo.mVaseRule, '+'))
				{
					if (aItem.size() < 5 || aItem[3] != '*')
						continue;
					int aCount = 0;
					VxParseInt(aItem.substr(4), aCount);
					for (int i = 0; i < aCount; i++)
						aPool.mItems.push_back({aItem.substr(0, 3), "q"});
				}
				theOut.push_back(aPool);
				return true;
			}
			if (aCode == 2)
			{
				for (const std::string& aPart : VxSplit(theInfo.mVaseRule, ';'))
				{
					if (aPart.size() < 5 || aPart[1] != '@' || aPart[3] != ':')
						continue;
					VxPotPool aPool;
					aPool.mCategory = aPart.substr(0, 1);
					std::string aContentType = aPart.substr(2, 1);
					for (const std::string& aItem : VxSplit(aPart.substr(4), '+'))
					{
						if (aItem.size() < 5 || aItem[3] != '*')
							continue;
						int aCount = 0;
						VxParseInt(aItem.substr(4), aCount);
						for (int i = 0; i < aCount; i++)
							aPool.mItems.push_back({aItem.substr(0, 3), aContentType});
					}
					theOut.push_back(aPool);
				}
				return true;
			}
			if (aCode == 3)
			{
				for (const std::string& aPart : VxSplit(theInfo.mVaseRule, ';'))
				{
					if (aPart.size() < 3 || aPart[1] != ':')
						continue;
					VxPotPool aPool;
					aPool.mCategory = aPart.substr(0, 1);
					for (const std::string& aItem : VxSplit(aPart.substr(2), '+'))
					{
						if (aItem.size() < 6 || aItem[3] != '*' || aItem[aItem.size() - 2] != '@')
							continue;
						int aCount = 0;
						VxParseInt(aItem.substr(4, aItem.size() - 6), aCount);
						for (int i = 0; i < aCount; i++)
							aPool.mItems.push_back({aItem.substr(0, 3), aItem.substr(aItem.size() - 1)});
					}
					theOut.push_back(aPool);
				}
				return true;
			}
			return false;
		}
	}

	bool VxLoadLevelInfo(int theLevel, const std::filesystem::path& theCsvPath, VxLevelInfo& theOut)
	{
		std::vector<std::vector<std::string>> aRows;
		if (!VxLoadCsv(theCsvPath, aRows))
			return false;
		theOut = VxLevelInfo();
		if (aRows.empty())
			return true;
		std::vector<std::string> aHeader = aRows[0];
		for (std::string& aName : aHeader)
			aName = VxTrim(aName);
		auto aCol = [&](const char* theName) -> int
		{
			for (size_t i = 0; i < aHeader.size(); i++)
			{
				if (aHeader[i] == theName)
					return static_cast<int>(i);
			}
			return -1;
		};
		int aKeyCol = aCol("key");
		if (aKeyCol < 0)
			return true;
		std::string aLevelKey = std::to_string(theLevel);
		const std::vector<std::string>* aRow = nullptr;
		for (size_t i = 1; i < aRows.size(); i++)
		{
			if (static_cast<int>(aRows[i].size()) > aKeyCol && VxTrim(aRows[i][aKeyCol]) == aLevelKey)
			{
				aRow = &aRows[i];
				break;
			}
		}
		if (!aRow)
			return true; // vx: level not configured; mValid stays false
		auto aCell = [&](const char* theName) -> std::string
		{
			int aIndex = aCol(theName);
			if (aIndex < 0 || static_cast<size_t>(aIndex) >= aRow->size())
				return std::string();
			return VxTrim((*aRow)[aIndex]);
		};
		theOut.mValid = true;
		VxParseInt(aCell("code1"), theOut.mCode1);
		VxParseInt(aCell("code2"), theOut.mCode2);
		theOut.mSpecial = aCell("special") == "1";
		VxParseInt(aCell("scene_id"), theOut.mSceneId);
		theOut.mSceneDesign = aCell("scene_design");
		VxParseInt(aCell("scene_rule_code"), theOut.mSceneRuleCode);
		theOut.mSceneRule = aCell("scene_rule");
		theOut.mVaseDesign = aCell("vase_design");
		VxParseInt(aCell("vase_rule_code"), theOut.mVaseRuleCode);
		theOut.mVaseRule = aCell("vase_rule");
		theOut.mSlotRule = aCell("slot_rule");
		theOut.mRandomSeeds = aCell("random_seeds");
		theOut.mStatistics = aCell("statistics");
		return true;
	}

	bool VxGeneratePots(const VxLevelInfo& theInfo, int theSeed, std::vector<VxPotDef>& theOut)
	{
		theOut.clear();
		std::vector<VxPotPool> aPools;
		if (!VxParseVaseRules(theInfo, aPools) || aPools.empty())
			return false;
		std::mt19937 aRng(static_cast<std::uint32_t>(theSeed));
		for (VxPotPool& aPool : aPools)
			std::shuffle(aPool.mItems.begin(), aPool.mItems.end(), aRng);
		for (int aRow = 0; aRow < 5; aRow++)
		{
			for (int aCol = 0; aCol < 9; aCol++)
			{
				size_t aIndex = static_cast<size_t>(aRow) * 10 + aCol;
				if (aIndex >= theInfo.mVaseDesign.size())
					continue;
				char aCategory = theInfo.mVaseDesign[aIndex];
				VxPotPool* aPool = nullptr;
				for (VxPotPool& aCandidate : aPools)
				{
					if (aCandidate.mCategory == std::string(1, aCategory))
					{
						aPool = &aCandidate;
						break;
					}
				}
				if (aCategory == '0' || !aPool || aPool->mItems.empty())
					continue;
				auto aItem = aPool->mItems.front();
				aPool->mItems.erase(aPool->mItems.begin());
				int aCode = VxAliasToCode(aItem.first);
				VxPotDef aDef;
				aDef.mRow = aRow;
				aDef.mCol = aCol;
				aDef.mCount = 1;
				aDef.mZombie = static_cast<int>(ZombieType::ZOMBIE_INVALID);
				aDef.mSeed = static_cast<int>(SeedType::SEED_NONE);
				if (aCode == -1)
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_NONE);
				else if (aCode < 0)
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_SUN);
				else if (aCode < 100)
				{
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_SEED);
					aDef.mSeed = aCode;
				}
				else
				{
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_ZOMBIE);
					aDef.mZombie = aCode - 100;
				}
				theOut.push_back(aDef);
			}
		}
		return true;
	}

	bool VxBuildScene(const VxLevelInfo& theInfo, int theSeed, std::vector<VxSceneDef>& theOut)
	{
		theOut.clear();
		std::vector<std::vector<std::vector<std::string>>> anArray;
		std::vector<VxSceneRule> aRules;
		if (!VxParseSceneRules(theInfo, anArray, aRules) || anArray.empty())
			return false;
		// vx: copy the pools so shuffling does not touch the rules (Python: {k: v[:] for ...})
		std::vector<VxSceneRule> aPools = aRules;
		std::mt19937 aRng(static_cast<std::uint32_t>(theSeed));
		for (VxSceneRule& aPool : aPools)
			std::shuffle(aPool.mPool.begin(), aPool.mPool.end(), aRng);
		for (size_t aRow = 0; aRow < anArray.size(); aRow++)
		{
			for (size_t aCol = 0; aCol < anArray[aRow].size(); aCol++)
			{
				for (const std::string& aName0 : anArray[aRow][aCol])
				{
					std::string aName = aName0;
					for (VxSceneRule& aPool : aPools)
					{
						if (aName0 == aPool.mKey && !aPool.mPool.empty())
						{
							aName = aPool.mPool.front();
							aPool.mPool.erase(aPool.mPool.begin());
							break;
						}
					}
					int aCode = VxAliasToCode(aName);
					if (aCode < 0)
						continue;
					VxSceneDef aDef;
					aDef.mRow = static_cast<int>(aRow);
					aDef.mCol = static_cast<int>(aCol);
					if (aCode < 100)
					{
						aDef.mIsPlant = true;
						aDef.mSeed = aCode;
						aDef.mZombie = static_cast<int>(ZombieType::ZOMBIE_INVALID);
					}
					else
					{
						aDef.mIsPlant = false;
						aDef.mZombie = aCode - 100;
						aDef.mSeed = static_cast<int>(SeedType::SEED_NONE);
					}
					theOut.push_back(aDef);
				}
			}
		}
		return true;
	}

	bool VxParseSlot(const VxLevelInfo& theInfo, int& theSun, std::vector<int>& theSlots)
	{
		theSlots.clear();
		if (theInfo.mSlotRule.empty())
			return false;
		std::string aSunStr;
		std::string aSlotsStr = theInfo.mSlotRule;
		size_t aDollar = theInfo.mSlotRule.find('$');
		if (aDollar != std::string::npos)
		{
			aSunStr = theInfo.mSlotRule.substr(0, aDollar);
			aSlotsStr = theInfo.mSlotRule.substr(aDollar + 1);
		}
		theSun = 0;
		if (!aSunStr.empty() && !VxParseInt(aSunStr, theSun))
			return false;
		for (const std::string& aAlias : VxSplit(aSlotsStr, '+'))
		{
			std::string aTrimmed = VxTrim(aAlias);
			if (aTrimmed.empty())
				continue;
			int aCode = VxAliasToCode(aTrimmed);
			if (aCode >= 0 && aCode < 100)
				theSlots.push_back(aCode);
		}
		return true;
	}

	bool VxParseRandomSeeds(const VxLevelInfo& theInfo, std::vector<int>& theOut)
	{
		theOut.clear();
		for (const std::string& aPart : VxSplit(theInfo.mRandomSeeds, ';'))
		{
			std::string aTrimmed = VxTrim(aPart);
			if (!aTrimmed.empty())
			{
				int aValue = 0;
				if (VxParseInt(aTrimmed, aValue))
					theOut.push_back(aValue);
			}
		}
		return true;
	}

	bool VxParseStatistics(const VxLevelInfo& theInfo, int& theAvgMs, int& theMaxMs, int& theSun)
	{
		theAvgMs = -1;
		theMaxMs = -1;
		theSun = 0;
		if (theInfo.mStatistics.empty())
			return false;
		std::vector<std::string> aParts = VxSplit(theInfo.mStatistics, ';');
		if (aParts.size() < 3)
			return false;
		try
		{
			// vx: mirror Python int(round(float(x) * 1000)) with round-half-to-even
			auto aRoundHalfEven = [](double theValue) -> int
			{
				double aFloor = std::floor(theValue);
				double aFrac = theValue - aFloor;
				if (aFrac < 0.5)
					return static_cast<int>(aFloor);
				if (aFrac > 0.5)
					return static_cast<int>(aFloor) + 1;
				return static_cast<int>(aFloor) + (static_cast<long long>(aFloor) % 2 != 0 ? 1 : 0);
			};
			theAvgMs = aRoundHalfEven(std::stod(VxTrim(aParts[0])) * 1000.0);
			theMaxMs = aRoundHalfEven(std::stod(VxTrim(aParts[1])) * 1000.0);
			theSun = std::stoi(VxTrim(aParts[2]));
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
}