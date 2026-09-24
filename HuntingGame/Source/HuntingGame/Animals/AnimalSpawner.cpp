#include "Animals/AnimalSpawner.h"
#include "Animals/WildAnimal.h"
#include "HuntingGame.h"

#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"

AAnimalSpawner::AAnimalSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	if (UBillboardComponent* Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard")))
	{
		Billboard->SetupAttachment(RootComponent);
	}
#endif
	AnimalClass = AWildAnimal::StaticClass();
}

void AAnimalSpawner::BeginPlay()
{
	Super::BeginPlay();

	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!AnimalClass || !Nav)
	{
		UE_LOG(LogHunting, Warning, TEXT("%s: missing AnimalClass or NavMesh (place a NavMeshBoundsVolume)."), *GetName());
		return;
	}

	const float HalfHeight = AnimalClass->GetDefaultObject<AWildAnimal>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const int32 Count = FMath::RandRange(MinHerdSize, FMath::Max(MinHerdSize, MaxHerdSize));

	for (int32 i = 0; i < Count; ++i)
	{
		FNavLocation NavLoc;
		if (!Nav->GetRandomReachablePointInRadius(GetActorLocation(), SpawnRadius, NavLoc))
		{
			continue;
		}
		const FTransform SpawnTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), NavLoc.Location + FVector(0.f, 0.f, HalfHeight));

		AWildAnimal* Animal = GetWorld()->SpawnActorDeferred<AWildAnimal>(AnimalClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Animal)
		{
			continue;
		}
		if (SpeciesOverride)
		{
			Animal->Species = SpeciesOverride;
		}
		Animal->FinishSpawning(SpawnTransform);
	}
}
