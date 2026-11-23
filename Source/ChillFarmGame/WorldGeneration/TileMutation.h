#pragma once

#include "CoreMinimal.h"
#include "TileData.h"
#include "TileMutationHistory/ETileMutationSource.h"


// A set of changes to apply to one tile.
//
// Only the fields that were set are written; everything else keeps its current value.
// The setters and the apply code are generated from CFG_FOREACH_TILE_VALUE_FIELD, so a field
// cannot be settable and then silently ignored when applied (which is what happened before).
//
//     FTileMutation().SetType(ETileType::Sand).SetMoisture(80)
//
struct FTileMutation
{
#define CFG_TILE_MUTATION_SETTER(FieldType, FieldName) \
	FTileMutation& Set##FieldName(FieldType InValue) \
	{ \
		FieldName = InValue; \
		SetFields |= ETileField::FieldName; \
		return *this; \
	}
	CFG_FOREACH_TILE_VALUE_FIELD(CFG_TILE_MUTATION_SETTER)
#undef CFG_TILE_MUTATION_SETTER

	FTileMutation& SetFlag(ETileFlags InFlag, bool bEnabled)
	{
		const uint8 Bit = static_cast<uint8>(InFlag);
		if (bEnabled)
		{
			FlagsToSet |= Bit;
			FlagsToClear &= ~Bit;
		}
		else
		{
			FlagsToClear |= Bit;
			FlagsToSet &= ~Bit;
		}
		return *this;
	}

	bool IsEmpty() const { return SetFields == ETileField::None && FlagsToSet == 0 && FlagsToClear == 0; }

	// Writes the requested values into the tile and returns the fields whose value really changed.
	// Setting a field to the value it already has is not a change.
	ETileField ApplyTo(FTileData& InOutTile) const
	{
		const FTileData Before = InOutTile;

#define CFG_TILE_MUTATION_APPLY(FieldType, FieldName) \
		if (EnumHasAnyFlags(SetFields, ETileField::FieldName)) { InOutTile.FieldName = FieldName; }
		CFG_FOREACH_TILE_VALUE_FIELD(CFG_TILE_MUTATION_APPLY)
#undef CFG_TILE_MUTATION_APPLY

		InOutTile.Flags = static_cast<uint8>((InOutTile.Flags | FlagsToSet) & ~FlagsToClear);

		return TileData::Diff(Before, InOutTile);
	}

private:
#define CFG_TILE_MUTATION_VALUE(FieldType, FieldName) \
	FieldType FieldName = {};
	CFG_FOREACH_TILE_VALUE_FIELD(CFG_TILE_MUTATION_VALUE)
#undef CFG_TILE_MUTATION_VALUE

	ETileField SetFields = ETileField::None;
	uint8 FlagsToSet = 0;
	uint8 FlagsToClear = 0;
};


// Who changed a tile and why. Attached to every change so tools can explain the current state.
struct FTileChangeContext
{
	ETileMutationSource Source = ETileMutationSource::Unknown;

	// The step that made the change: a generation pass, a gameplay action ("Till", "Water"), an editor tool.
	FName Step = NAME_None;

	// Optional detail about this particular change ("Next to water", "Isolated tile").
	FName Reason = NAME_None;

	// The object that asked for the change. Weak: it may be destroyed later while the change is still listed.
	TWeakObjectPtr<const UObject> Instigator = nullptr;
};


// One applied change: the tile before and after, which fields differ, and the context.
struct FTileChange
{
	FIntPoint Coordinates = FIntPoint::ZeroValue;
	FTileData Before;
	FTileData After;
	ETileField ChangedFields = ETileField::None;
	FTileChangeContext Context;
};
