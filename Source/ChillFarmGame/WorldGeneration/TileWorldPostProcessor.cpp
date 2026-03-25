#include "TileWorldPostProcessor.h"

#include "ETileType.h"
#include "TileDataGrid.h"
#include "BiomeTypeData.h"
#include "TileMutationHistory/TileMutationHelper.h"


void UTileWorldPostProcessor::ProcessWorld(UTileDataGrid* TileGrid)
{
	//ApplyBiomeSmoothing(TileGrid);
	ApplyShoreSmoothing(TileGrid);
	ApplyTileSmoothing(TileGrid);
}

void UTileWorldPostProcessor::ApplyBiomeSmoothing(UTileDataGrid* TileGrid)
{
	if (!TileGrid) return;

    TArray<FIntPoint> ToChange;
    TArray<ETileType> NewTypes;

    const auto GridSize = TileGrid->GetSize();

    for (auto X = 1; X < GridSize.X - 1; ++X)
    {
        for (auto Y = 1; Y < GridSize.Y - 1; ++Y)
        {
            const auto& Tile = TileGrid->GetTileChecked(X, Y);
            if (!Tile->GetBiomeData()) continue;

            TMap<EBiomeType, int32> BiomeCount;
        	
            for (auto DX = -1; DX <= 1; ++DX)
            {
                for (auto DY = -1; DY <= 1; ++DY)
                {
                    if (DX == 0 && DY == 0)
                        continue;

                    const auto OffsetTile = TileGrid->GetTileChecked(X + DX, Y + DY);
                    if (!OffsetTile || !OffsetTile->GetBiomeData()) continue;

                    BiomeCount.FindOrAdd(OffsetTile->GetBiomeData()->BiomeType)++;
                }
            }

            if (BiomeCount.IsEmpty()) continue;
        	
            auto DominantBiome = Tile->GetBiomeData()->BiomeType;
            auto MaxCount = 0;

            for (const auto& [Biome, Count] : BiomeCount)
            {
                if (Count > MaxCount)
                {
                    MaxCount = Count;
                    DominantBiome = Biome;
                }
            }
        	
            constexpr auto MinMajority = 5;

            if (DominantBiome != Tile->GetBiomeData()->BiomeType && MaxCount >= MinMajority)
            {
                for (const auto& [From, To] : Tile->GetBiomeData()->SmoothingOverrides)
                {
                    if (Tile->GetType() == From)
                    {
                        ToChange.Add({X, Y});
                        NewTypes.Add(To);
                        break;
                    }
                }
            }
        }
    }
	
    for (auto Index = 0; Index < ToChange.Num(); ++Index)
    {
        const auto& TileCoordinates = ToChange[Index];
    	auto BiomeData = TileGrid->GetTileChecked(TileCoordinates.X, TileCoordinates.Y)->GetBiomeData();
    	// TODO Mutation: Actual phases and reasons
    	FTileMutationHelper TileMutation;
    	auto MutationContext = FTileMutationContext::Make(TileCoordinates, ETileMutationSource::PostProcess, BiomeData, this, TEXT("Smoothing"), TEXT("Biome Smoothing"));
    	TileMutation.SetType(NewTypes[Index]);
    	TileGrid->MutateTile(MutationContext, TileMutation);
    }
}

void UTileWorldPostProcessor::ApplyShoreSmoothing(UTileDataGrid* TileGrid)
{
	TArray<FIntPoint> ShoreTileCoordinates;

	auto GridSize = TileGrid->GetSize();
	for (auto X = 0; X < GridSize.X; ++X)
	{
		for (auto Y = 0; Y < GridSize.Y; ++Y)
		{
			const auto& Tile = TileGrid->GetTileChecked(X, Y);
			if (Tile->GetType() == ETileType::Water) continue;

			if (TileGrid->HasNeighborOfType(X, Y, ETileType::Water)) ShoreTileCoordinates.Add({X, Y}); 
		}
	}

	for (const auto& TileCoordinates : ShoreTileCoordinates)
	{
		auto Tile = TileGrid->GetTileChecked(TileCoordinates);
		if (!Tile->GetBiomeData())
		{
			continue;
		}
		// TODO Mutation: Actual phases and reasons
		auto BiomeData = Tile->GetBiomeData();
		FTileMutationHelper TileMutation;
		auto MutationContext = FTileMutationContext::Make(TileCoordinates, ETileMutationSource::PostProcess, BiomeData, this, TEXT("Smoothing"), TEXT("Shore Smoothing"));
		TileMutation.SetType(BiomeData->ShoreTile);
		TileGrid->MutateTile(MutationContext, TileMutation);
	}
}

void UTileWorldPostProcessor::ApplyTileSmoothing(UTileDataGrid* TileGrid)
{
	TArray<FIntPoint> ToSmooth;

	auto GridSize = TileGrid->GetSize();
	for (auto X = 0; X < GridSize.X; ++X)
		for (auto Y = 0; Y < GridSize.Y; ++Y)
		{
			const auto& Tile = TileGrid->GetTileChecked(X, Y);

			auto Same = 0;
			auto Total = 0;

			for (const auto& Offset : { FIntPoint(1,0), {-1,0}, {0,1}, {0,-1} })
			{
				const auto OffsetTile = TileGrid->GetTileChecked(X + Offset.X, Y + Offset.Y);
				if (!OffsetTile) continue;

				Total++;
				if (OffsetTile->GetType() == Tile->GetType()) Same++;
			}

			if (Total > 1 && Same <= 1 && Tile->GetSmoothingOverride() != ETileType::ETileType_MAX)
			{
				ToSmooth.Add({X, Y});
			}
		}

	for (const auto& TileCoordinates : ToSmooth)
	{
		const auto& Tile = TileGrid->GetTileChecked(TileCoordinates.X, TileCoordinates.Y);
		// TODO Mutation: Actual phases and reasons
		FTileMutationHelper TileMutation;
		
		auto MutationContext = FTileMutationContext::Make(TileCoordinates, ETileMutationSource::PostProcess, Tile->GetBiomeData(), this, TEXT("Smoothing"), TEXT("Tile Smoothing"));
		TileMutation.SetType(Tile->GetSmoothingOverride());
		TileGrid->MutateTile(MutationContext, TileMutation);
	}
}