#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/HuntingTypes.h"
#include "HunterCharacter.generated.h"

class UCameraComponent;
class UInventoryComponent;
class AHuntingWeapon;
class AWildAnimal;

struct FHuntMessage
{
	FString Text;
	float ExpireTime = 0.f;
};

/**
 * 一人称のハンター。姿勢・移動速度が動物に見つかりやすさ/聞かれやすさを決め、
 * 心拍数・呼吸・姿勢が照準の揺れを決める。
 */
UCLASS()
class HUNTINGGAME_API AHunterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHunterCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ---- 動物の感覚から参照される値 ----
	/** 足音の大きさ 0..1 */
	float GetNoiseLevel() const;
	/** 見えやすさ (姿勢・迷彩) */
	float GetVisibilityFactor() const;
	/** 動きの大きさ 0..1 (弓を引く動作も含む) */
	float GetMovementFactor() const;
	/** 匂いの強さ (汗で増え、消臭で減る) */
	float GetScentLevel() const { return ScentLevel; }
	FVector GetEyeLocation() const;
	bool IsDead() const { return bDead; }

	// ---- 武器 ----
	FRotator GetAimRotation() const;
	FVector GetAimLocation() const;
	bool IsAiming() const { return bAiming; }
	void OnWeaponFired(float RecoilPitchDeg);
	void NotifyThreatened();

	// ---- HUD ----
	void PushMessage(const FString& Text, float Duration = 4.f);
	const TArray<FHuntMessage>& GetMessages() const { return Messages; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	float GetHeartRate() const { return HeartRate; }
	float GetBreathFraction() const { return BreathRemaining / MaxBreathHold; }
	bool IsHoldingBreath() const { return bHoldingBreath; }
	EHunterStance GetStance() const { return Stance; }
	AHuntingWeapon* GetWeapon() const { return Weapon; }
	float GetHarvestProgress() const;
	bool IsInventoryVisible() const { return bShowInventory; }
	UInventoryComponent* GetInventory() const { return Inventory; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hunter")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hunter")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(EditAnywhere, Category = "Hunter|Weapon")
	TArray<TSubclassOf<AHuntingWeapon>> WeaponClasses;

	// ---- 移動 (cm/s) ----
	UPROPERTY(EditAnywhere, Category = "Hunter|Movement")
	float WalkSpeed = 160.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Movement")
	float SprintSpeed = 550.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Movement")
	float CrouchSpeed = 90.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Movement")
	float ProneSpeed = 35.f;

	// ---- 身体 ----
	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float SprintStaminaCost = 12.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float RestingHeartRate = 65.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float MaxHeartRate = 175.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float MaxBreathHold = 8.f;

	/** 静止・立ち姿勢・安静時の照準揺れ (度) */
	UPROPERTY(EditAnywhere, Category = "Hunter|Body")
	float BaseSwayDeg = 0.6f;

	// ---- 装備 ----
	/** 迷彩服: 1 = 普段着, 0.6 = 迷彩 */
	UPROPERTY(EditAnywhere, Category = "Hunter|Gear")
	float CamoFactor = 0.7f;

	/** 消臭: 1 = なし, 0.5 = 消臭スプレー */
	UPROPERTY(EditAnywhere, Category = "Hunter|Gear")
	float ScentControl = 1.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Camera")
	float DefaultFOV = 80.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Camera")
	float LookSensitivity = 1.f;

	UPROPERTY(EditAnywhere, Category = "Hunter|Interaction")
	float InteractRange = 350.f;

protected:
	virtual void BeginPlay() override;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void FirePressed();
	void FireReleased();
	void AimPressed();
	void AimReleased();
	void ShiftPressed();
	void ShiftReleased();
	void ToggleCrouch();
	void ToggleProne();
	void ReloadPressed();
	void InteractPressed();
	void JumpPressed();
	void ToggleInventory();
	void SwitchWeapon();

	void EquipWeapon(int32 Index);
	void SetStance(EHunterStance NewStance);
	void UpdateMovement(float Dt);
	void UpdatePhysiology(float Dt);
	void UpdateSway(float Dt);
	void UpdateCamera(float Dt);
	void UpdateHarvest(float Dt);
	void CheckBuckFever();
	bool CanAct() const;

	UPROPERTY()
	TObjectPtr<AHuntingWeapon> Weapon;

	UPROPERTY()
	TArray<TObjectPtr<AHuntingWeapon>> Weapons;

	int32 WeaponIndex = 0;

	EHunterStance Stance = EHunterStance::Standing;
	bool bSprintHeld = false;
	bool bAiming = false;
	bool bHoldingBreath = false;
	bool bDead = false;
	bool bShowInventory = false;

	float Health = 100.f;
	float Stamina = 100.f;
	float HeartRate = 65.f;
	float Adrenaline = 0.f;
	float Sweat = 0.f;
	float ScentLevel = 1.f;
	float BreathRemaining = 8.f;
	float BreathLockout = 0.f;

	float SwayTime = 0.f;
	float BreathPhase = 0.f;
	float PulsePhase = 0.f;
	FRotator LastSway = FRotator::ZeroRotator;
	float LookMotion = 0.f;
	float BuckFeverTimer = 0.f;

	TWeakObjectPtr<AWildAnimal> HarvestTarget;
	float HarvestElapsed = 0.f;

	TArray<FHuntMessage> Messages;
};
