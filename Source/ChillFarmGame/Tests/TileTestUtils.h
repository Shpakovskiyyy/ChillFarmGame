#pragma once

// Helpers shared by the tile world specs. Test-only: include it from Tests/*.cpp files only.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TileGrid.h"
#include "TileGridLayout.h"
#include "TileGenerationRule.h"
#include "TilesGenerationRuleSet.h"
#include "BiomeTypeData.h"
#include "BiomeTypeDataSet.h"

#include <initializer_list>

namespace TileTestUtils
{
	// Flags for every spec in this module: editor context, product (game) tests.
	inline constexpr EAutomationTestFlags SpecFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	// Readable values for failure messages.
	inline FString ToTestString(int32 InValue) { return FString::FromInt(InValue); }
	inline FString ToTestString(uint8 InValue) { return FString::FromInt(InValue); }
	inline FString ToTestString(bool bInValue) { return bInValue ? TEXT("true") : TEXT("false"); }
	inline FString ToTestString(const FString& InValue) { return InValue; }
	inline FString ToTestString(const FIntPoint& InValue) { return FString::Printf(TEXT("(%d, %d)"), InValue.X, InValue.Y); }

	inline FString ToTestString(ETileField InFields)
	{
		if (InFields == ETileField::None) return TEXT("None");

		FString Result;
		for (const ETileField Field : { ETileField::Type, ETileField::Biome, ETileField::Height, ETileField::Fertility, ETileField::Moisture, ETileField::Flags })
		{
			if (!EnumHasAnyFlags(InFields, Field)) continue;
			if (!Result.IsEmpty()) Result += TEXT("|");
			Result += TileData::GetFieldName(Field);
		}
		return Result;
	}

	// Reflected (UENUM) enums print their value name.
	template <typename EnumType>
	typename TEnableIf<TIsEnum<EnumType>::Value, FString>::Type ToTestString(EnumType InValue)
	{
		return UEnum::GetValueAsString(InValue);
	}

	// Equality check that only needs operator== and prints both values on failure. Used instead of
	// FAutomationTestBase::TestEqual for enums, points and bytes, whose overloads differ between
	// engine versions.
	template <typename ValueType>
	bool TestEq(FAutomationTestBase& Test, const FString& What, const ValueType& Actual, const ValueType& Expected)
	{
		if (Actual == Expected) return true;
		Test.AddError(FString::Printf(TEXT("%s: Expected %s, got %s"), *What, *ToTestString(Expected), *ToTestString(Actual)));
		return false;
	}

	inline FTileGridLayout MakeLayout(int32 SizeX, int32 SizeY, const FVector& Origin = FVector::ZeroVector)
	{
		FTileGridLayout Layout;
		Layout.Size = FIntPoint(SizeX, SizeY);
		Layout.TileSize = 100.f;
		Layout.LevelHeight = 50.f;
		Layout.Origin = Origin;
		return Layout;
	}

	// G grass, D dirt, S sand, R rock, W water, '.' not generated.
	inline ETileType CharToType(TCHAR InChar)
	{
		switch (InChar)
		{
			case 'G': return ETileType::Grass;
			case 'D': return ETileType::Dirt;
			case 'S': return ETileType::Sand;
			case 'R': return ETileType::Rock;
			case 'W': return ETileType::Water;
			default:  return ETileType::ETileType_MAX;
		}
	}

	inline TCHAR TypeToChar(ETileType InType)
	{
		switch (InType)
		{
			case ETileType::Grass: return 'G';
			case ETileType::Dirt:  return 'D';
			case ETileType::Sand:  return 'S';
			case ETileType::Rock:  return 'R';
			case ETileType::Water: return 'W';
			default:               return '.';
		}
	}

	inline int32 RowLength(const TCHAR* InRow)
	{
		int32 Length = 0;
		while (InRow[Length] != 0) ++Length;
		return Length;
	}

	// Builds a grid from rows of tile characters, first row is Y = 0. Land tiles get LandBiome,
	// water tiles get the Water biome.
	inline FTileGrid MakeGrid(std::initializer_list<const TCHAR*> InRows, EBiomeType LandBiome = EBiomeType::Plains)
	{
		const int32 Height = static_cast<int32>(InRows.size());
		const int32 Width = Height > 0 ? RowLength(*InRows.begin()) : 0;

		FTileGrid Grid(MakeLayout(Width, Height));
		int32 Y = 0;
		for (const TCHAR* Row : InRows)
		{
			check(RowLength(Row) == Width);
			for (int32 X = 0; X < Width; ++X)
			{
				const ETileType Type = CharToType(Row[X]);
				if (Type == ETileType::ETileType_MAX) continue;

				Grid.ApplyMutation(FIntPoint(X, Y), FTileMutation()
					.SetType(Type)
					.SetBiome(Type == ETileType::Water ? EBiomeType::Water : LandBiome));
			}
			++Y;
		}
		return Grid;
	}

	// Sets height levels from rows of digits, same shape as the grid.
	inline void SetHeights(FTileGrid& InOutGrid, std::initializer_list<const TCHAR*> InRows)
	{
		int32 Y = 0;
		for (const TCHAR* Row : InRows)
		{
			for (int32 X = 0; X < RowLength(Row); ++X)
			{
				InOutGrid.ApplyMutation(FIntPoint(X, Y), FTileMutation().SetHeight(static_cast<uint8>(Row[X] - '0')));
			}
			++Y;
		}
	}

	// The grid's tile types as rows joined with '/', e.g. "GGW/GSW". Readable in test failures.
	inline FString DescribeTypes(const FTileGrid& InGrid)
	{
		FString Result;
		const FIntPoint Size = InGrid.GetSize();
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			if (Y > 0) Result += TEXT("/");
			for (int32 X = 0; X < Size.X; ++X)
			{
				Result.AppendChar(TypeToChar(InGrid.GetTile(FIntPoint(X, Y)).Type));
			}
		}
		return Result;
	}

	inline FTileGenerationRule MakeRule(ETileType InType, ETileNoiseSource InSource, float InMin, float InMax)
	{
		FTileGenerationRule Rule;
		Rule.TileType = InType;
		Rule.NoiseSource = InSource;
		Rule.MinNoise = InMin;
		Rule.MaxNoise = InMax;
		return Rule;
	}

	// Data assets are created as transient objects. Create and use them inside one test body: they
	// are not rooted and may be collected between frames.
	inline UTilesGenerationRuleSet* MakeRuleSet(std::initializer_list<FTileGenerationRule> InRules)
	{
		UTilesGenerationRuleSet* RuleSet = NewObject<UTilesGenerationRuleSet>();
		for (const FTileGenerationRule& Rule : InRules) RuleSet->Rules.Add(Rule);
		return RuleSet;
	}

	inline UBiomeTypeData* MakeBiome(EBiomeType InType, const FVector2D& InHeightRange, const FVector2D& InMoistureRange,
		UTilesGenerationRuleSet* InRules = nullptr, uint8 InFertility = 50, ETileType InShore = ETileType::Sand)
	{
		UBiomeTypeData* Biome = NewObject<UBiomeTypeData>();
		Biome->BiomeType = InType;
		Biome->HeightRange = InHeightRange;
		Biome->MoistureRange = InMoistureRange;
		Biome->TilesGenerationRuleSet = InRules;
		Biome->BaseFertility = InFertility;
		Biome->ShoreTile = InShore;
		return Biome;
	}

	inline UBiomeTypeDataSet* MakeBiomeSet(std::initializer_list<UBiomeTypeData*> InBiomes)
	{
		UBiomeTypeDataSet* Set = NewObject<UBiomeTypeDataSet>();
		for (UBiomeTypeData* Biome : InBiomes) Set->BiomeData.Add(Biome);
		return Set;
	}

	// Two land biomes split by moisture plus water, with rules that use every tile type. Good enough
	// to run the whole pipeline: water below 0.3, plains (dry) and forest (wet) above.
	struct FTestWorldData
	{
		UBiomeTypeDataSet* Biomes = nullptr;
		UTilesGenerationRuleSet* DefaultRules = nullptr;
	};

	inline FTestWorldData MakeTestWorldData()
	{
		UTilesGenerationRuleSet* PlainsRules = MakeRuleSet({
			MakeRule(ETileType::Rock, ETileNoiseSource::Height, 0.8f, 1.01f),
			MakeRule(ETileType::Dirt, ETileNoiseSource::Secondary, 0.0f, 0.2f),
			MakeRule(ETileType::Grass, ETileNoiseSource::Height, 0.0f, 1.01f),
		});
		UTilesGenerationRuleSet* ForestRules = MakeRuleSet({
			MakeRule(ETileType::Dirt, ETileNoiseSource::Secondary, 0.0f, 0.35f),
			MakeRule(ETileType::Grass, ETileNoiseSource::Height, 0.0f, 1.01f),
		});

		FTestWorldData Data;
		Data.DefaultRules = MakeRuleSet({ MakeRule(ETileType::Grass, ETileNoiseSource::Height, 0.0f, 1.01f) });
		Data.Biomes = MakeBiomeSet({
			MakeBiome(EBiomeType::Water,  FVector2D(0.0, 0.3), FVector2D(0.0, 1.0)),
			MakeBiome(EBiomeType::Plains, FVector2D(0.3, 1.0), FVector2D(0.0, 0.5), PlainsRules, 60),
			MakeBiome(EBiomeType::Forest, FVector2D(0.3, 1.0), FVector2D(0.5, 1.0), ForestRules, 80),
		});
		return Data;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
