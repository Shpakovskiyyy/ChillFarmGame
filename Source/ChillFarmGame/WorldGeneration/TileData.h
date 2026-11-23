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


// The four corners of a tile, as bits of FTileData::Slope. North is -Y, the direction the grid is
// stored in.
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETileCorner : uint8
{
	None		= 0 UMETA(Hidden),
	NorthWest	= 1 << 0,
	NorthEast	= 1 << 1,
	SouthEast	= 1 << 2,
	SouthWest	= 1 << 3,
};
ENUM_CLASS_FLAGS(ETileCorner)


// A cardinal direction on the grid. North is -Y.
UENUM(BlueprintType)
enum class ETileDirection : uint8
{
	North,
	East,
	South,
	West,
};


// What a slope looks like, from which of its corners are raised (see TileData::GetSlopeShape).
enum class ETileSlopeShape : uint8
{
	// No corner raised.
	Flat,

	// The two corners of one edge raised: the tile rises towards that neighbour. A ramp is one of these.
	Edge,

	// One corner raised: the outside of a hill's corner.
	OuterCorner,

	// Three corners raised: the inside of a hill's corner.
	InnerCorner,

	// Two opposite corners, or all four. Generation never makes these.
	Invalid,
};


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
	Slope		= 1 << 6,
};
ENUM_CLASS_FLAGS(ETileField)


// Logical data of a single tile.
//
// Plain values only, on purpose: no UObject pointers and nothing that can be derived from other
// tiles. That keeps a tile at seven bytes, lets the whole grid be copied, compared, hashed and saved
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

	// Bitmask of ETileCorner: the corners that are one level higher than Height. 0 is a flat tile.
	// A slope joins the tile to the higher ground next to it; the surface runs straight between corners.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (Bitmask, BitmaskEnum = "/Script/ChillFarmGame.ETileCorner"))
	uint8 Slope = 0;

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
	bool IsSlope() const { return Slope != 0; }
	bool HasRaisedCorner(ETileCorner InCorner) const { return (Slope & static_cast<uint8>(InCorner)) != 0; }

	bool operator==(const FTileData& Other) const
	{
		return Type == Other.Type && Biome == Other.Biome && Height == Other.Height && Slope == Other.Slope
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
	X(uint8,      Slope) \
	X(uint8,      Fertility) \
	X(uint8,      Moisture)

// Fails to compile when FTileData grows without this file being revisited.
static_assert(sizeof(FTileData) == 7, "FTileData changed: update ETileField and CFG_FOREACH_TILE_VALUE_FIELD, then this assert.");


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

	// Grid offset of a direction: the neighbour on that side.
	inline FIntPoint GetDirectionOffset(ETileDirection InDirection)
	{
		switch (InDirection)
		{
			case ETileDirection::North: return FIntPoint(0, -1);
			case ETileDirection::East:  return FIntPoint(1, 0);
			case ETileDirection::South: return FIntPoint(0, 1);
			default:                    return FIntPoint(-1, 0);
		}
	}

	// Offset from the tile's centre to a corner, in half tiles: (-1, -1) is north-west.
	inline FIntPoint GetCornerOffset(ETileCorner InCorner)
	{
		switch (InCorner)
		{
			case ETileCorner::NorthWest: return FIntPoint(-1, -1);
			case ETileCorner::NorthEast: return FIntPoint(1, -1);
			case ETileCorner::SouthEast: return FIntPoint(1, 1);
			case ETileCorner::SouthWest: return FIntPoint(-1, 1);
			default:                     return FIntPoint::ZeroValue;
		}
	}

	inline constexpr ETileCorner AllCorners[] = { ETileCorner::NorthWest, ETileCorner::NorthEast, ETileCorner::SouthEast, ETileCorner::SouthWest };

	// The two corners on the tile's edge towards a cardinal offset. The first is the western one on a
	// north or south edge and the northern one on an east or west edge, so the corners a tile and its
	// neighbour share come out in the same order from both sides.
	inline void GetEdgeCorners(const FIntPoint& InOffset, ETileCorner& OutFirst, ETileCorner& OutSecond)
	{
		if (InOffset.Y < 0)      { OutFirst = ETileCorner::NorthWest; OutSecond = ETileCorner::NorthEast; }
		else if (InOffset.Y > 0) { OutFirst = ETileCorner::SouthWest; OutSecond = ETileCorner::SouthEast; }
		else if (InOffset.X > 0) { OutFirst = ETileCorner::NorthEast; OutSecond = ETileCorner::SouthEast; }
		else                     { OutFirst = ETileCorner::NorthWest; OutSecond = ETileCorner::SouthWest; }
	}

	// The level a corner of the tile is at.
	inline int32 GetCornerLevel(const FTileData& InTile, ETileCorner InCorner)
	{
		return InTile.Height + (InTile.HasRaisedCorner(InCorner) ? 1 : 0);
	}

	// The highest point of the tile's surface.
	inline int32 GetTopLevel(const FTileData& InTile)
	{
		return InTile.Height + (InTile.IsSlope() ? 1 : 0);
	}

	// Slope mask of a ramp: the edge towards the cardinal offset raised.
	inline uint8 MakeEdgeSlope(const FIntPoint& InTowards)
	{
		ETileCorner First, Second;
		GetEdgeCorners(InTowards, First, Second);
		return static_cast<uint8>(First | Second);
	}

	inline ETileSlopeShape GetSlopeShape(uint8 InSlope)
	{
		if (InSlope > 0xF) return ETileSlopeShape::Invalid;
		switch (FMath::CountBits(InSlope))
		{
			case 0: return ETileSlopeShape::Flat;
			case 1: return ETileSlopeShape::OuterCorner;
			case 3: return ETileSlopeShape::InnerCorner;
			case 2:
			{
				// Opposite corners (north-west with south-east, north-east with south-west) are not an edge.
				constexpr uint8 Diagonal1 = static_cast<uint8>(ETileCorner::NorthWest) | static_cast<uint8>(ETileCorner::SouthEast);
				constexpr uint8 Diagonal2 = static_cast<uint8>(ETileCorner::NorthEast) | static_cast<uint8>(ETileCorner::SouthWest);
				return InSlope == Diagonal1 || InSlope == Diagonal2 ? ETileSlopeShape::Invalid : ETileSlopeShape::Edge;
			}
			default: return ETileSlopeShape::Invalid;
		}
	}

	// Which way a slope faces, as an offset from the tile's centre: the direction of the raised edge
	// for an edge, the raised corner for an outer corner, the one low corner for an inner corner
	// (corners in half tiles, see GetCornerOffset). Zero for flat and invalid slopes.
	inline FIntPoint GetSlopeFacing(uint8 InSlope)
	{
		const ETileSlopeShape Shape = GetSlopeShape(InSlope);
		if (Shape == ETileSlopeShape::Flat || Shape == ETileSlopeShape::Invalid) return FIntPoint::ZeroValue;

		FIntPoint Sum = FIntPoint::ZeroValue;
		for (const ETileCorner Corner : AllCorners)
		{
			const bool bRaised = (InSlope & static_cast<uint8>(Corner)) != 0;
			if (bRaised != (Shape == ETileSlopeShape::InnerCorner)) Sum += GetCornerOffset(Corner);
		}

		// The two corners of an edge add up to twice the edge's direction.
		return Shape == ETileSlopeShape::Edge ? FIntPoint(Sum.X / 2, Sum.Y / 2) : Sum;
	}

	// Converts a 0..1 value to the 0..100 scale Fertility and Moisture are stored in.
	inline uint8 ToPercent(float InValue01)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(InValue01 * 100.f), 0, 100));
	}
}
