#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/HuntingTypes.h"
#include "WildAnimal.generated.h"

class UAnimalSpeciesData;
class ATrackSign;
class AHunterCharacter;
class UInventoryComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimalStateChanged, EAnimalState, NewState);

/**
 * 野生動物。視覚・聴覚・嗅覚 (風) で警戒度を蓄積し、種の気質に応じて逃走/反撃する。
 * 被弾は弾道が通過した臓器で判定し、出血で徐々に弱って倒れる (即死は脳・一部のみ)。
 */
UCLASS()
class HUNTINGGAME_API AWildAnimal : public ACharacter
{
	GENERATED_BODY()

public:
	AWildAnimal();

	virtual void Tick(float DeltaSeconds) override;

	void ApplyBallisticHit(const FHitResult& Hit, const FVector& Direction, float ImpactEnergyJ, const FAmmoSpec& Ammo, AActor* Shooter);

	/** 銃声・弦音など。AudibleRangeCm 内なら反応する */
	void HearLoudNoise(const FVector& Location, float AudibleRangeCm);

	void ReceiveHerdAlarm(const FVector& InThreatLocation);

	UFUNCTION(BlueprintPure, Category = "Animal")
	bool IsDead() const { return State == EAnimalState::Dead; }

	UFUNCTION(BlueprintPure, Category = "Animal")
	bool CanBeHarvested() const { return IsDead() && !bHarvested; }

	UFUNCTION(BlueprintPure, Category = "Animal")
	EAnimalState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Animal")
	float GetMeatQuality() const { return MeatQuality; }

	/** アニメーション BP 用: 0 = 静止, 1 = 全力疾走 */
	UFUNCTION(BlueprintPure, Category = "Animal")
	float GetGaitAlpha() const;

	FString Harvest(UInventoryComponent* Inventory);
	float GetHarvestSeconds() const;
	FText GetSpeciesName() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animal")
	TObjectPtr<UAnimalSpeciesData> Species;

	UPROPERTY(EditAnywhere, Category = "Animal|Tracks")
	TSubclassOf<ATrackSign> FootprintClass;

	UPROPERTY(EditAnywhere, Category = "Animal|Tracks")
	TSubclassOf<ATrackSign> BloodClass;

	UPROPERTY(EditAnywhere, Category = "Animal|Tracks")
	TSubclassOf<ATrackSign> BedClass;

	UPROPERTY(EditAnywhere, Category = "Animal|Tracks")
	float FootprintSpacing = 350.f;

	/** ハンターからこの距離以内のときだけ足跡を残す (アクター数の抑制) */
	UPROPERTY(EditAnywhere, Category = "Animal|Tracks")
	float FootprintSpawnRadius = 20000.f;

	/** 目の位置のソケット。無ければアクター前方上部を使う */
	UPROPERTY(EditAnywhere, Category = "Animal|Senses")
	FName EyeSocketName = TEXT("head");

	/** 臓器の当たり判定を表示 (配置調整用) */
	UPROPERTY(EditAnywhere, Category = "Animal|Debug")
	bool bDebugDrawVitals = false;

	UPROPERTY(BlueprintAssignable, Category = "Animal")
	FOnAnimalStateChanged OnStateChanged;

	UFUNCTION(BlueprintImplementableEvent, Category = "Animal")
	void OnHitReaction(EHitZone Zone);

	/** 警戒音 (鼻を鳴らす・足を踏み鳴らす)。サウンド/アニメを BP で再生 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Animal")
	void OnAlarmCall();

	UFUNCTION(BlueprintImplementableEvent, Category = "Animal")
	void OnAttack();

	UFUNCTION(BlueprintImplementableEvent, Category = "Animal")
	void OnDied();

protected:
	virtual void BeginPlay() override;

	/** メッシュ未設定でもテストできる仮の胴体と頭 */
	UPROPERTY(VisibleAnywhere, Category = "Animal|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, Category = "Animal|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderHead;

private:
	void UpdateWounds(float Dt);
	void UpdateSenses(float Dt);
	void UpdateBehavior(float Dt);
	void UpdateLocomotion(float Dt);
	void UpdateTracks();

	void SetState(EAnimalState NewState);
	void ReactToThreat();
	void FleeFrom(const FVector& From);
	void StartCharge();
	void UpdateCharge(float Dt);
	void StartWander();
	void AlarmHerd();
	void FaceLocation(const FVector& Target, float Dt);
	void Die();

	bool MoveTo(const FVector& Destination);
	bool IsMoveFinished() const;
	void StopMoving();

	bool HasLineOfSight(const FVector& From, const AActor* Target, const FVector& TargetPoint) const;
	FVector GetEyeLocation() const;
	bool IsWounded() const { return BleedRate > 0.f || Health < MaxHealthCached * 0.9f; }

	TArray<EHitZone> TraceWoundChannel(const FVector& Entry, const FVector& Direction, float DepthCm) const;
	EHitZone ResolveBoneZone(FName BoneName) const;
	FVector GetOrganWorldLocation(const FVitalOrgan& Organ) const;

	void SpawnTrackSign(TSubclassOf<ATrackSign> SignClass, ETrackSignType Type, float Intensity);

	AHunterCharacter* GetHunter() const;

	EAnimalState State = EAnimalState::Grazing;
	float StateTime = 0.f;
	float NextDecisionTime = 5.f;

	float MaxHealthCached = 100.f;
	float Health = 100.f;
	float BleedRate = 0.f;
	float Mobility = 1.f;
	float MeatQuality = 1.f;
	float Stamina = 30.f;
	float Awareness = 0.f;
	float AttackCooldown = 0.f;
	float BloodAccumulator = 0.f;
	float LastBloodSpawnTime = -10.f;
	bool bIncapacitated = false;
	bool bHarvested = false;
	EBloodSign CurrentBloodSign = EBloodSign::None;

	FVector ThreatLocation = FVector::ZeroVector;
	FVector HomeLocation = FVector::ZeroVector;
	FVector LastFootprintLocation = FVector::ZeroVector;
};
