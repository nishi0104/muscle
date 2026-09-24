#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HuntingTypes.h"
#include "TrackSign.generated.h"

class UDecalComponent;
class UMaterialInterface;

/**
 * 地面に残る痕跡 (足跡・血痕・寝床)。時間とともに薄れて消える。
 * ハンターは調べる (E) ことで、種・新しさ・進行方向・血の性状を読み取れる。
 */
UCLASS()
class HUNTINGGAME_API ATrackSign : public AActor
{
	GENERATED_BODY()

public:
	ATrackSign();

	void InitSign(ETrackSignType InType, EBloodSign InBlood, const FText& InSpecies, float InIntensity, float InHeadingDeg, bool bInRunning);

	FText Describe() const;

	ETrackSignType GetSignType() const { return SignType; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(EditDefaultsOnly, Category = "Track")
	TObjectPtr<UMaterialInterface> FootprintMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Track")
	TObjectPtr<UMaterialInterface> BedMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Track")
	TMap<EBloodSign, TObjectPtr<UMaterialInterface>> BloodMaterials;

	UPROPERTY(EditDefaultsOnly, Category = "Track")
	float LifetimeSeconds = 300.f;

private:
	ETrackSignType SignType = ETrackSignType::Footprint;
	EBloodSign BloodSign = EBloodSign::None;
	FText SpeciesName;
	float Intensity = 1.f;
	float HeadingDeg = 0.f;
	bool bRunning = false;
	float SpawnTime = 0.f;
};
