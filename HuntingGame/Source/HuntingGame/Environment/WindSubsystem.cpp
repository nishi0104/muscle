#include "Environment/WindSubsystem.h"

void UWindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Seed = FMath::FRandRange(0.f, 1000.f);
	PrevailingHeadingDeg = FMath::FRandRange(0.f, 360.f);
	MeanSpeedMS = FMath::FRandRange(1.5f, 5.f);
}

void UWindSubsystem::Tick(float DeltaTime)
{
	Time += DeltaTime;

	// 数分周期の風向の振れ (±60°) + 地形で巻く短周期の揺らぎ (±15°)
	const float Shift = FMath::PerlinNoise1D(Seed + Time * 0.01f) * 60.f;
	const float Swirl = FMath::PerlinNoise1D(Seed * 2.f + Time * 0.15f) * 15.f;
	CurrentHeadingDeg = FRotator::NormalizeAxis(PrevailingHeadingDeg + Shift + Swirl);

	const float Drift = 1.f + 0.3f * FMath::PerlinNoise1D(Seed * 3.f + Time * 0.05f);
	const float Gust = FMath::Max(0.f, FMath::PerlinNoise1D(Seed * 4.f + Time * 0.3f)) * MeanSpeedMS * 0.8f;
	CurrentSpeedMS = FMath::Max(0.f, MeanSpeedMS * Drift + Gust);
}

TStatId UWindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWindSubsystem, STATGROUP_Tickables);
}

FVector UWindSubsystem::GetWindDirection() const
{
	return FRotator(0.f, CurrentHeadingDeg, 0.f).Vector();
}

void UWindSubsystem::SetPrevailingWind(float HeadingDeg, float InMeanSpeedMS)
{
	PrevailingHeadingDeg = HeadingDeg;
	MeanSpeedMS = FMath::Max(0.f, InMeanSpeedMS);
}
