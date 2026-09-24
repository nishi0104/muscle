#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HuntingTypes.h"
#include "HuntingWeapon.generated.h"

class ABallisticProjectile;
class UStaticMeshComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EWeaponKind : uint8
{
	Rifle,
	Bow
};

/**
 * ボルトアクションライフル / コンパウンドボウ。
 * ライフル: 撃つたびにボルト操作、弾倉が空なら装填。
 * 弓: 押している間に引き絞り、離して射る。引いたまま長く保持すると腕が震える。
 */
UCLASS()
class HUNTINGGAME_API AHuntingWeapon : public AActor
{
	GENERATED_BODY()

public:
	AHuntingWeapon();

	virtual void Tick(float DeltaSeconds) override;

	void PressTrigger();
	void ReleaseTrigger();
	void Reload();

	bool IsBusy() const { return BusyTimeRemaining > 0.f; }
	bool IsDrawing() const { return bDrawing; }
	float GetSwayMultiplier() const;
	float GetAimFOV() const { return AimFOV; }
	FString GetStatusText() const;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	EWeaponKind Kind = EWeaponKind::Rifle;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FText DisplayName = NSLOCTEXT("Hunting", "Rifle", "Bolt-action .308");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FAmmoSpec Ammo;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ABallisticProjectile> ProjectileClass;

	/** ライフル: 弾倉容量 / 弓: 常に 1 (番えた矢) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	int32 MagazineCapacity = 4;

	/** 予備弾 / 矢筒の本数 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	int32 ReserveAmmo = 16;

	/** ボルト操作の時間 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float CycleTime = 1.1f;

	/** ライフル: 弾倉装填 / 弓: 次の矢を番える */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float ReloadTime = 3.5f;

	/** 機械精度 (MOA)。1 MOA ≒ 100m で 2.9cm */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float AccuracyMOA = 1.5f;

	/** 構えずに撃ったときの追加の散布 (度) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float HipSpreadDeg = 3.f;

	/** 照準と弾道が交わる距離 (ゼロイン) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float ZeroDistanceM = 100.f;

	/** 発射音で動物が反応する距離 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float AudibleRangeCm = 60000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float RecoilPitchDeg = 3.f;

	/** 覗いたときの視野角 (スコープ倍率の代わり) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float AimFOV = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Bow")
	float FullDrawTime = 1.2f;

	/** これを超えて引き続けると震えが増す */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Bow")
	float ComfortHoldTime = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> CycleSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> DryFireSound;

protected:
	virtual void BeginPlay() override;

private:
	enum class EBusyReason : uint8 { None, Cycling, Reloading, Nocking };

	void FireShot(float VelocityScale);
	void StartBusy(EBusyReason Reason, float Time);
	void FinishBusy();

	int32 Loaded = 0;
	float BusyTimeRemaining = 0.f;
	EBusyReason BusyReason = EBusyReason::None;
	bool bDrawing = false;
	float DrawFraction = 0.f;
	float HoldTime = 0.f;
};

/** コンパウンドボウ (70lb) + カーボン矢 420gr の既定値 */
UCLASS()
class HUNTINGGAME_API AHuntingBow : public AHuntingWeapon
{
	GENERATED_BODY()

public:
	AHuntingBow();
};
