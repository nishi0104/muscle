#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HuntingTypes.h"
#include "BallisticProjectile.generated.h"

class UStaticMeshComponent;

/**
 * 重力・空気抵抗・横風を数値積分する弾/矢。
 * 着弾時の運動エネルギーで貫通力を決め、動物側で臓器判定を行う。
 */
UCLASS()
class HUNTINGGAME_API ABallisticProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABallisticProjectile();

	virtual void Tick(float DeltaSeconds) override;

	void Launch(const FAmmoSpec& InAmmo, const FVector& InVelocityCmS, AActor* InShooter);

	/** 着弾エフェクト・サウンド用 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void OnImpact(const FHitResult& Hit, float EnergyJ);

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxRangeCm = 150000.f;

	/** 空気密度 kg/m^3 (海面 15℃) */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float AirDensity = 1.225f;

private:
	void HandleImpact(const FHitResult& Hit);

	FAmmoSpec Ammo;
	FVector VelocityMS = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Shooter;
	float TraveledCm = 0.f;
	bool bInFlight = false;
};
