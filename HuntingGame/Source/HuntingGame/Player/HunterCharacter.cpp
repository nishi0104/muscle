#include "Player/HunterCharacter.h"
#include "Animals/WildAnimal.h"
#include "Items/InventoryComponent.h"
#include "Tracking/TrackSign.h"
#include "Weapons/HuntingWeapon.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	// 目の高さ (カメラの相対 Z)。しゃがみ/伏せでは Capsule が縮むぶん下がる
	constexpr float StandEyeZ = 64.f;
	constexpr float CrouchEyeZ = 35.f;
	constexpr float ProneEyeZ = -20.f;
}

AHunterCharacter::AHunterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(40.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, StandEyeZ));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(DefaultFOV);

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));

	bUseControllerRotationYaw = true;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->NavAgentProps.bCanCrouch = true;
	Move->SetCrouchedHalfHeight(55.f);
	Move->JumpZVelocity = 380.f;
	Move->BrakingDecelerationWalking = 1200.f;
	Move->MaxAcceleration = 1200.f;

	WeaponClasses.Add(AHuntingWeapon::StaticClass());
	WeaponClasses.Add(AHuntingBow::StaticClass());
}

void AHunterCharacter::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	Stamina = MaxStamina;
	HeartRate = RestingHeartRate;
	BreathRemaining = MaxBreathHold;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	for (const TSubclassOf<AHuntingWeapon>& WeaponClass : WeaponClasses)
	{
		if (!WeaponClass)
		{
			continue;
		}
		if (AHuntingWeapon* NewWeapon = GetWorld()->SpawnActor<AHuntingWeapon>(WeaponClass, Params))
		{
			NewWeapon->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			NewWeapon->SetActorRelativeLocation(FVector(35.f, 12.f, -18.f));
			NewWeapon->SetActorHiddenInGame(true);
			Weapons.Add(NewWeapon);
		}
	}
	EquipWeapon(0);

	PushMessage(TEXT("Watch the wind. Move slowly. Read the blood."), 6.f);
}

void AHunterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AHunterCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AHunterCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AHunterCharacter::Turn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AHunterCharacter::LookUp);

	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AHunterCharacter::FirePressed);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AHunterCharacter::FireReleased);
	PlayerInputComponent->BindAction(TEXT("Aim"), IE_Pressed, this, &AHunterCharacter::AimPressed);
	PlayerInputComponent->BindAction(TEXT("Aim"), IE_Released, this, &AHunterCharacter::AimReleased);
	PlayerInputComponent->BindAction(TEXT("SprintOrHoldBreath"), IE_Pressed, this, &AHunterCharacter::ShiftPressed);
	PlayerInputComponent->BindAction(TEXT("SprintOrHoldBreath"), IE_Released, this, &AHunterCharacter::ShiftReleased);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AHunterCharacter::ToggleCrouch);
	PlayerInputComponent->BindAction(TEXT("Prone"), IE_Pressed, this, &AHunterCharacter::ToggleProne);
	PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &AHunterCharacter::ReloadPressed);
	PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AHunterCharacter::InteractPressed);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AHunterCharacter::JumpPressed);
	PlayerInputComponent->BindAction(TEXT("ToggleInventory"), IE_Pressed, this, &AHunterCharacter::ToggleInventory);
	PlayerInputComponent->BindAction(TEXT("SwitchWeapon"), IE_Pressed, this, &AHunterCharacter::SwitchWeapon);
}

void AHunterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	Messages.RemoveAll([Now](const FHuntMessage& M) { return M.ExpireTime < Now; });

	if (bDead)
	{
		return;
	}

	UpdateMovement(DeltaSeconds);
	UpdatePhysiology(DeltaSeconds);
	UpdateSway(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateHarvest(DeltaSeconds);

	LookMotion = FMath::Max(0.f, LookMotion - DeltaSeconds * 2.f);
}

// ------------------------------------------------------------------
// 入力
// ------------------------------------------------------------------

bool AHunterCharacter::CanAct() const
{
	return !bDead && !HarvestTarget.IsValid();
}

void AHunterCharacter::MoveForward(float Value)
{
	if (Value != 0.f && CanAct())
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AHunterCharacter::MoveRight(float Value)
{
	if (Value != 0.f && CanAct())
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AHunterCharacter::Turn(float Value)
{
	if (Value == 0.f || bDead)
	{
		return;
	}
	const float Scale = bAiming ? Camera->FieldOfView / DefaultFOV : 1.f;
	AddControllerYawInput(Value * LookSensitivity * Scale);
	// 素早く首を振るのも動物からは「動き」に見える
	LookMotion = FMath::Min(1.f, LookMotion + FMath::Abs(Value) * 0.02f);
}

void AHunterCharacter::LookUp(float Value)
{
	if (Value == 0.f || bDead)
	{
		return;
	}
	const float Scale = bAiming ? Camera->FieldOfView / DefaultFOV : 1.f;
	AddControllerPitchInput(Value * LookSensitivity * Scale);
}

void AHunterCharacter::FirePressed()
{
	if (Weapon && CanAct())
	{
		Weapon->PressTrigger();
	}
}

void AHunterCharacter::FireReleased()
{
	if (Weapon && !bDead)
	{
		Weapon->ReleaseTrigger();
	}
}

void AHunterCharacter::AimPressed()
{
	if (CanAct())
	{
		bAiming = true;
		bSprintHeld = false;
	}
}

void AHunterCharacter::AimReleased()
{
	bAiming = false;
	bHoldingBreath = false;
}

void AHunterCharacter::ShiftPressed()
{
	if (bAiming)
	{
		if (BreathLockout <= 0.f && BreathRemaining > 0.5f)
		{
			bHoldingBreath = true;
		}
	}
	else
	{
		bSprintHeld = true;
	}
}

void AHunterCharacter::ShiftReleased()
{
	bSprintHeld = false;
	bHoldingBreath = false;
}

void AHunterCharacter::ToggleCrouch()
{
	if (CanAct())
	{
		SetStance(Stance == EHunterStance::Crouching ? EHunterStance::Standing : EHunterStance::Crouching);
	}
}

void AHunterCharacter::ToggleProne()
{
	if (CanAct())
	{
		SetStance(Stance == EHunterStance::Prone ? EHunterStance::Crouching : EHunterStance::Prone);
	}
}

void AHunterCharacter::SetStance(EHunterStance NewStance)
{
	Stance = NewStance;
	if (Stance == EHunterStance::Standing)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AHunterCharacter::ReloadPressed()
{
	if (Weapon && CanAct())
	{
		Weapon->Reload();
	}
}

void AHunterCharacter::JumpPressed()
{
	if (CanAct() && Stance == EHunterStance::Standing && Stamina > 15.f)
	{
		Stamina -= 10.f;
		Jump();
	}
}

void AHunterCharacter::ToggleInventory()
{
	bShowInventory = !bShowInventory;
}

void AHunterCharacter::SwitchWeapon()
{
	if (Weapons.Num() > 1 && CanAct() && (!Weapon || (!Weapon->IsBusy() && !Weapon->IsDrawing())))
	{
		EquipWeapon((WeaponIndex + 1) % Weapons.Num());
	}
}

void AHunterCharacter::EquipWeapon(int32 Index)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return;
	}
	if (Weapon)
	{
		Weapon->SetActorHiddenInGame(true);
	}
	WeaponIndex = Index;
	Weapon = Weapons[Index];
	Weapon->SetActorHiddenInGame(false);
}

void AHunterCharacter::InteractPressed()
{
	if (!CanAct())
	{
		return;
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * InteractRange;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Interact), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	if (AWildAnimal* Animal = bHit ? Cast<AWildAnimal>(Hit.GetActor()) : nullptr)
	{
		if (Animal->CanBeHarvested())
		{
			HarvestTarget = Animal;
			HarvestElapsed = 0.f;
			bAiming = false;
			PushMessage(FString::Printf(TEXT("Field dressing the %s..."), *Animal->GetSpeciesName().ToString()), Animal->GetHarvestSeconds());
		}
		else if (!Animal->IsDead())
		{
			PushMessage(TEXT("It's still alive. Back off."), 2.f);
		}
		return;
	}

	// 見ている地点の周囲にある痕跡を調べる
	const FVector LookPoint = bHit ? FVector(Hit.ImpactPoint) : End;
	ATrackSign* Nearest = nullptr;
	float NearestDist = 150.f;
	for (TActorIterator<ATrackSign> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), LookPoint);
		// 血痕は足跡より優先して読む
		const float Bias = It->GetSignType() == ETrackSignType::Footprint ? 30.f : 0.f;
		if (Dist + Bias < NearestDist)
		{
			NearestDist = Dist + Bias;
			Nearest = *It;
		}
	}
	PushMessage(Nearest ? Nearest->Describe().ToString() : TEXT("Nothing notable here."), Nearest ? 8.f : 2.f);
}

// ------------------------------------------------------------------
// 状態更新
// ------------------------------------------------------------------

void AHunterCharacter::UpdateMovement(float Dt)
{
	const float Speed2D = GetVelocity().Size2D();
	const bool bSprinting = bSprintHeld && Stance == EHunterStance::Standing && Stamina > 0.f && Speed2D > 10.f && !bAiming;

	float Target = WalkSpeed;
	switch (Stance)
	{
	case EHunterStance::Crouching: Target = CrouchSpeed; break;
	case EHunterStance::Prone:     Target = ProneSpeed; break;
	default: if (bSprinting) { Target = SprintSpeed; } break;
	}
	if (bAiming)
	{
		Target *= 0.5f;
	}
	// 息が上がると足が鈍る
	Target *= FMath::Lerp(0.6f, 1.f, Stamina / MaxStamina);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = Target;
	Move->MaxWalkSpeedCrouched = Target;

	if (bSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - SprintStaminaCost * Dt);
	}
	else if (!bHoldingBreath)
	{
		const float Regen = Speed2D < 10.f ? 8.f : 4.f;
		Stamina = FMath::Min(MaxStamina, Stamina + Regen * Dt);
	}
}

void AHunterCharacter::UpdatePhysiology(float Dt)
{
	const float Speed2D = GetVelocity().Size2D();
	const bool bSprinting = bSprintHeld && Speed2D > WalkSpeed * 1.2f;

	// 心拍数: 運動・疲労・興奮 (バックフィーバー) で上がる
	float Target = RestingHeartRate;
	Target += bSprinting ? (MaxHeartRate - RestingHeartRate) : Speed2D / WalkSpeed * 20.f;
	Target += (1.f - Stamina / MaxStamina) * 40.f;
	Target += Adrenaline;
	Target = FMath::Min(Target, MaxHeartRate + 15.f);
	HeartRate = FMath::FInterpConstantTo(HeartRate, Target, Dt, Target > HeartRate ? 12.f : 4.f);

	Adrenaline = FMath::Max(0.f, Adrenaline - 3.f * Dt);

	BuckFeverTimer -= Dt;
	if (BuckFeverTimer <= 0.f)
	{
		BuckFeverTimer = 0.5f;
		CheckBuckFever();
	}

	// 息止め: 限界を超えると大きく息を吐いて揺れが増す
	BreathLockout = FMath::Max(0.f, BreathLockout - Dt);
	if (bHoldingBreath)
	{
		BreathRemaining -= Dt;
		if (BreathRemaining <= 0.f)
		{
			BreathRemaining = 0.f;
			bHoldingBreath = false;
			BreathLockout = 3.f;
			Adrenaline += 12.f;
			PushMessage(TEXT("You gasp for air."), 2.f);
		}
	}
	else if (BreathLockout <= 0.f)
	{
		BreathRemaining = FMath::Min(MaxBreathHold, BreathRemaining + Dt * 0.8f);
	}

	// 汗をかくと匂いが強くなる
	const float Exertion = FMath::Clamp((HeartRate - RestingHeartRate) / (MaxHeartRate - RestingHeartRate), 0.f, 1.f);
	Sweat = FMath::Clamp(Sweat + (Exertion * 0.01f - 0.002f) * Dt, 0.f, 0.6f);
	ScentLevel = ScentControl * (1.f + Sweat);
}

void AHunterCharacter::CheckBuckFever()
{
	if (!bAiming)
	{
		return;
	}
	// 大物をスコープに捉えると心拍が跳ね上がる
	const FVector Eye = GetEyeLocation();
	const FVector Forward = Camera->GetForwardVector();
	for (TActorIterator<AWildAnimal> It(GetWorld()); It; ++It)
	{
		if (It->IsDead())
		{
			continue;
		}
		const FVector To = It->GetActorLocation() - Eye;
		if (To.Size() < 15000.f && FVector::DotProduct(To.GetSafeNormal(), Forward) > 0.94f)
		{
			Adrenaline = FMath::Max(Adrenaline, 25.f);
			return;
		}
	}
}

void AHunterCharacter::UpdateSway(float Dt)
{
	AController* PC = GetController();
	if (!PC)
	{
		return;
	}

	SwayTime += Dt;

	float StanceMul = 1.f;
	switch (Stance)
	{
	case EHunterStance::Crouching: StanceMul = 0.55f; break;
	case EHunterStance::Prone:     StanceMul = 0.25f; break;
	default: break;
	}
	const float HeartMul = HeartRate / RestingHeartRate;
	const float WeaponMul = Weapon ? Weapon->GetSwayMultiplier() : 1.f;
	const float MoveMul = 1.f + GetVelocity().Size2D() / WalkSpeed * 2.f;
	const float Amp = BaseSwayDeg * StanceMul * HeartMul * WeaponMul * MoveMul;

	// 呼吸 (安静時 毎分 12 回前後、運動後は速く深い)
	const float BreathHz = 0.2f + (HeartRate - RestingHeartRate) / 400.f;
	BreathPhase += Dt * BreathHz * UE_TWO_PI;
	const float BreathAmp = bHoldingBreath ? 0.1f : 1.f;

	// 脈拍ごとの小さな跳ね
	PulsePhase += Dt * HeartRate / 60.f * UE_TWO_PI;
	const float Pulse = FMath::Pow(FMath::Max(0.f, FMath::Sin(PulsePhase)), 8.f) * Amp * 0.25f;

	const float Pitch = FMath::Sin(BreathPhase) * Amp * BreathAmp
		+ FMath::PerlinNoise1D(SwayTime * 0.9f + 37.f) * Amp * 0.4f
		+ Pulse;
	const float Yaw = FMath::PerlinNoise1D(SwayTime * 0.7f) * Amp * 0.8f;

	// 揺れは実際の照準方向を動かす (プレイヤーが補正する)
	const FRotator Sway(Pitch, Yaw, 0.f);
	const FRotator Delta = Sway - LastSway;
	LastSway = Sway;
	PC->SetControlRotation(PC->GetControlRotation() + Delta);
}

void AHunterCharacter::UpdateCamera(float Dt)
{
	const float TargetFOV = (bAiming && Weapon) ? Weapon->GetAimFOV() : DefaultFOV;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, Dt, 10.f));

	float EyeZ = StandEyeZ;
	if (bIsCrouched)
	{
		EyeZ = Stance == EHunterStance::Prone ? ProneEyeZ : CrouchEyeZ;
	}
	FVector Loc = Camera->GetRelativeLocation();
	Loc.Z = FMath::FInterpTo(Loc.Z, EyeZ, Dt, 6.f);
	Camera->SetRelativeLocation(Loc);
}

void AHunterCharacter::UpdateHarvest(float Dt)
{
	AWildAnimal* Target = HarvestTarget.Get();
	if (!Target)
	{
		HarvestTarget.Reset();
		return;
	}
	HarvestElapsed += Dt;
	if (HarvestElapsed >= Target->GetHarvestSeconds())
	{
		PushMessage(Target->Harvest(Inventory), 6.f);
		HarvestTarget.Reset();
	}
}

float AHunterCharacter::GetHarvestProgress() const
{
	const AWildAnimal* Target = HarvestTarget.Get();
	return Target ? FMath::Clamp(HarvestElapsed / Target->GetHarvestSeconds(), 0.f, 1.f) : -1.f;
}

// ------------------------------------------------------------------
// 動物から見た値
// ------------------------------------------------------------------

float AHunterCharacter::GetNoiseLevel() const
{
	float StanceMul = 1.f;
	switch (Stance)
	{
	case EHunterStance::Crouching: StanceMul = 0.5f; break;
	case EHunterStance::Prone:     StanceMul = 0.3f; break;
	default: break;
	}
	const float SpeedFrac = FMath::Clamp(GetVelocity().Size2D() / SprintSpeed, 0.f, 1.f);
	return FMath::Clamp(FMath::Pow(SpeedFrac, 1.3f) * StanceMul, 0.f, 1.f);
}

float AHunterCharacter::GetVisibilityFactor() const
{
	float StanceVis = 1.f;
	switch (Stance)
	{
	case EHunterStance::Crouching: StanceVis = 0.55f; break;
	case EHunterStance::Prone:     StanceVis = 0.25f; break;
	default: break;
	}
	return StanceVis * CamoFactor;
}

float AHunterCharacter::GetMovementFactor() const
{
	const float Body = FMath::Clamp(GetVelocity().Size2D() / SprintSpeed, 0.f, 1.f);
	const float Draw = (Weapon && Weapon->IsDrawing()) ? 0.25f : 0.f;
	return FMath::Clamp(Body + Draw + LookMotion * 0.2f, 0.f, 1.f);
}

FVector AHunterCharacter::GetEyeLocation() const
{
	return Camera->GetComponentLocation();
}

FRotator AHunterCharacter::GetAimRotation() const
{
	return Camera->GetComponentRotation();
}

FVector AHunterCharacter::GetAimLocation() const
{
	return Camera->GetComponentLocation();
}

void AHunterCharacter::OnWeaponFired(float RecoilPitchDeg)
{
	if (AController* PC = GetController())
	{
		const float Kick = RecoilPitchDeg * (Stance == EHunterStance::Prone ? 0.6f : 1.f);
		PC->SetControlRotation(PC->GetControlRotation() + FRotator(Kick, FMath::FRandRange(-0.3f, 0.3f) * Kick, 0.f));
	}
	Adrenaline += 8.f;
	bHoldingBreath = false;
}

void AHunterCharacter::NotifyThreatened()
{
	Adrenaline = FMath::Max(Adrenaline, 45.f);
	PushMessage(TEXT("It's charging!"), 2.f);
}

float AHunterCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (bDead || Applied <= 0.f)
	{
		return Applied;
	}

	Health -= Applied;
	Adrenaline += 30.f;
	HarvestTarget.Reset();

	// 突き飛ばされる
	if (DamageCauser)
	{
		const FVector Push = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal2D() * 600.f + FVector(0.f, 0.f, 200.f);
		LaunchCharacter(Push, true, true);
	}

	if (Health <= 0.f)
	{
		bDead = true;
		bAiming = false;
		PushMessage(TEXT("You have been mortally wounded."), 5.f);
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			DisableInput(PC);
		}
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, [this]()
		{
			UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
		}, 5.f, false);
	}
	else
	{
		PushMessage(FString::Printf(TEXT("You're hurt! (%d / %d)"), FMath::RoundToInt(Health), FMath::RoundToInt(MaxHealth)), 2.f);
	}
	return Applied;
}

void AHunterCharacter::PushMessage(const FString& Text, float Duration)
{
	if (Text.IsEmpty())
	{
		return;
	}
	FHuntMessage Message;
	Message.Text = Text;
	Message.ExpireTime = GetWorld()->GetTimeSeconds() + Duration;
	Messages.Add(Message);
	if (Messages.Num() > 5)
	{
		Messages.RemoveAt(0);
	}
}
