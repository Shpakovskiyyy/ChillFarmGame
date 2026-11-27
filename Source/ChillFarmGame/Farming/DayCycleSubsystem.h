#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DayCycleSubsystem.generated.h"

class UCropSubsystem;
class UTileWorldSubsystem;


// A new day started. The argument is its number, the first day is 1.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDayStarted, int32);


// Game time in days, and what happens overnight.
//
// A day lasts CFG.Farming.DayLengthSeconds of real time (0 stops the clock); "CFG.Farming.NextDay"
// skips to the next morning. The night is one ordered step, because its parts depend on each other:
// first watered crops grow (they read the Watered flag), then the land dries (the flag is cleared on
// every tile, as a Simulation change), then the day number goes up.
UCLASS()
class CHILLFARMGAME_API UDayCycleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Uses these systems for the night. Done by Initialize in a real world; tests call it on their own.
	void Bind(UTileWorldSubsystem& InTileWorld, UCropSubsystem& InCrops);

public:
	int32 GetDay() const { return Day; }

	// 0 at dawn, 1 at the end of the day.
	float GetTimeOfDay01() const;

	// Runs the night and starts the next day.
	void StartNextDay();

public:
	FOnDayStarted OnDayStarted;

private:
	int32 Day = 1;
	float SecondsIntoDay = 0.f;

	TWeakObjectPtr<UTileWorldSubsystem> TileWorld;
	TWeakObjectPtr<UCropSubsystem> Crops;
};
