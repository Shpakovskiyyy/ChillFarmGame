#pragma once

#include "CoreMinimal.h"
#include "ETileType.h"
#include "EBiomeType.h"
#include "TileData.generated.h"


// Yes/no state of a tile, stored as bits in FTileData::Flags.
UENUM()
enum class ETileFlags : uint8
{
	None		= 0 UMETA(Hidden),

	// Something stands on the tile (a building, a rock, a tree) and nothing else can be placed there.
	Occupied	= 1 << 0,

	// Watered by the player today. Daily state, cleared overnight by the day cycle; separate from the
	// natural ground Moisture that generation stores.
	Watered		= 1 << 1,
};
ENUM_CLASS_FLAGS(ETileFlags)


// Names one field of FTileData. Combined as a bitmask to say which fields a mutation changed.
enum class ETileField : uint8
{
	None		= 0,
	Type		= 1 << 0,
	Biome		= 1 << 1,
	Height		= 1 << 2,
	Fertility	= 1 << 3,
	Moisture	= 1 << 4,
	Flags		= 1 << 5,
};
ENUM_CLASS_FLAGS(ETileField)


// Logical data of a single tile.
//
// Plain values only, on purpose: no UObject pointers and nothing that can be derived from other
// tiles. That keeps a tile at six bytes, lets the whole grid be copied, compared, hashed and saved
// as raw data, and lets generation code touch it without caring about the garbage collector.
//
// The fields are public, but a tile that belongs to a grid can only be reached as const:
// every change is an FTileMutation applied by FTileGrid::ApplyMutation, through
// UTileWorldSubsystem::MutateTile at runtime or FTileGenerationContext::Mutate during generation.
USTRUCT(BlueprintType)
struct FTileData
{
	GENERATED_BODY()

	// What the surface is made of. ETileType_MAX means "not generated yet".
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	ETileType Type = ETileType::ETileType_MAX;

	// The biome the tile was generated for. Look the asset up through UBiomeTypeDataSet::FindBiome.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	EBiomeType Biome = EBiomeType::EBiomeType_MAX;

	// Elevation as a whole number of levels. World height is Height * FTileGridLayout::LevelHeight.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	uint8 Height = 0;

	// 0..100
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	uint8 Fertility = 0;

	// 0..100
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	uint8 Moisture = 0;

	// Bitmask of ETileFlags.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	uint8 Flags = 0;

	bool IsGenerated() const { return Type != ETileType::ETileType_MAX; }
	bool IsWater() const { return Type == ETileType::Water; }
	bool HasFlag(ETileFlags InFlag) const { return (Flags & static_cast<uint8>(InFlag)) != 0; }

	bool operator==(const FTileData& Other) const
	{
		return Type == Other.Type && Biome == Other.Biome && Height == Other.Height
			&& Fertility == Other.Fertility && Moisture == Other.Moisture && Flags == Other.Flags;
	}
	bool operator!=(const FTileData& Other) const { return !(*this == Other); }
};


// The one list of FTileData fields that are set by value.
//
// FTileMutation (its setters and Apply), the diff helper below and the debug names are generated
// from this list, so a field cannot be handled in one place and forgotten in another. Flags is not
// part of it because flags are set and cleared bit by bit.
//
// Adding a field: add the member to FTileData, an entry to ETileField, and a line here.
#define CFG_FOREACH_TILE_VALUE_FIELD(X) \
	X(ETileType,  Type) \
	X(EBiomeType, Biome) \
	X(uint8,      Height) \
	X(uint8,      Fertility) \
	X(uint8,      Moisture)

// Fails to compile when FTileData grows without this file being revisited.
static_assert(sizeof(FTileData) == 6, "FTileData changed: update ETileField and CFG_FOREACH_TILE_VALUE_FIELD, then this assert.");


namespace TileData
{
	// Which fields differ between two tiles.
	inline ETileField Diff(const FTileData& A, const FTileData& B)
	{
		ETileField Result = ETileField::None;

#define CFG_DIFF_TILE_FIELD(FieldType, FieldName) \
		if (A.FieldName != B.FieldName) { Result |= ETileField::FieldName; }
		CFG_FOREACH_TILE_VALUE_FIELD(CFG_DIFF_TILE_FIELD)
#undef CFG_DIFF_TILE_FIELD

		if (A.Flags != B.Flags) { Result |= ETileField::Flags; }
		return Result;
	}

	// Name of a single field, for logs and tools.
	inline const TCHAR* GetFieldName(ETileField InField)
	{
		switch (InField)
		{
#define CFG_TILE_FIELD_NAME(FieldType, FieldName) \
			case ETileField::FieldName: return TEXT(#FieldName);
			CFG_FOREACH_TILE_VALUE_FIELD(CFG_TILE_FIELD_NAME)
#undef CFG_TILE_FIELD_NAME

			case ETileField::Flags: return TEXT("Flags");
			default: return TEXT("None");
		}
	}

	// Converts a 0..1 value to the 0..100 scale Fertility and Moisture are stored in.
	inline uint8 ToPercent(float InValue01)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(InValue01 * 100.f), 0, 100));
	}
}
