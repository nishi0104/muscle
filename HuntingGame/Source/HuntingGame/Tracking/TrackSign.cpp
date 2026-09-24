#include "Tracking/TrackSign.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"

ATrackSign::ATrackSign()
{
	PrimaryActorTick.bCanEverTick = false;

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	RootComponent = Decal;
	Decal->DecalSize = FVector(8.f, 12.f, 12.f);
}

void ATrackSign::InitSign(ETrackSignType InType, EBloodSign InBlood, const FText& InSpecies, float InIntensity, float InHeadingDeg, bool bInRunning)
{
	SignType = InType;
	BloodSign = InBlood;
	SpeciesName = InSpecies;
	Intensity = InIntensity;
	HeadingDeg = InHeadingDeg;
	bRunning = bInRunning;
	SpawnTime = GetWorld()->GetTimeSeconds();

	UMaterialInterface* Material = nullptr;
	switch (SignType)
	{
	case ETrackSignType::Footprint:
		Material = FootprintMaterial;
		Decal->DecalSize = FVector(6.f, 5.f, 8.f);
		break;
	case ETrackSignType::Blood:
		if (const TObjectPtr<UMaterialInterface>* Found = BloodMaterials.Find(BloodSign))
		{
			Material = *Found;
		}
		Decal->DecalSize = FVector(6.f, 4.f + 8.f * Intensity, 4.f + 8.f * Intensity);
		break;
	case ETrackSignType::Bed:
		Material = BedMaterial;
		Decal->DecalSize = FVector(10.f, 50.f, 80.f);
		break;
	}
	if (Material)
	{
		Decal->SetDecalMaterial(Material);
	}

	// 後半 40% の時間をかけて薄れ、消えたらアクターも破棄
	Decal->SetFadeOut(LifetimeSeconds * 0.6f, LifetimeSeconds * 0.4f, true);
}

static FString CompassFromHeading(float HeadingDeg)
{
	// Unreal の Yaw 0 = +X を北とみなす
	static const TCHAR* Names[] = { TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"),
		TEXT("south"), TEXT("south-west"), TEXT("west"), TEXT("north-west") };
	const float Normalized = FMath::Fmod(HeadingDeg + 360.f + 22.5f, 360.f);
	return Names[FMath::Clamp(FMath::FloorToInt(Normalized / 45.f), 0, 7)];
}

FText ATrackSign::Describe() const
{
	const float Age = GetWorld()->GetTimeSeconds() - SpawnTime;
	const FString Freshness =
		Age < 60.f ? TEXT("very fresh") :
		Age < 180.f ? TEXT("a few minutes old") : TEXT("old and fading");

	FString Text;
	switch (SignType)
	{
	case ETrackSignType::Footprint:
		Text = FString::Printf(TEXT("%s tracks, %s. Heading %s. %s"),
			*SpeciesName.ToString(), *Freshness, *CompassFromHeading(HeadingDeg),
			bRunning ? TEXT("Deep, widely spaced prints - it was running.") : TEXT("Even stride - it was walking calmly."));
		break;

	case ETrackSignType::Bed:
		Text = FString::Printf(TEXT("A flattened bed in the grass, %s. It lay down here - a wounded animal may be close. Consider waiting before pushing on."),
			*Freshness);
		break;

	case ETrackSignType::Blood:
	{
		FString Reading;
		switch (BloodSign)
		{
		case EBloodSign::BrightFrothy:
			Reading = TEXT("Pink, frothy blood with bubbles. Lung hit - it should not go far.");
			break;
		case EBloodSign::BrightRed:
			Reading = TEXT("Heavy spray of bright red blood. Heart or artery - follow quickly.");
			break;
		case EBloodSign::DarkRed:
			Reading = TEXT("Dark, deep red blood. Likely liver. Give it time before following.");
			break;
		case EBloodSign::GutMatter:
			Reading = TEXT("Dark blood mixed with stomach contents. Gut shot - wait a long time, or it will run for miles.");
			break;
		default:
			Reading = TEXT("A few scattered drops. Probably a muscle or leg wound - it may survive.");
			break;
		}
		Text = FString::Printf(TEXT("Blood, %s. %s"), *Freshness, *Reading);
		break;
	}
	}
	return FText::FromString(Text);
}
