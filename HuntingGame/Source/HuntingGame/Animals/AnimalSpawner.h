#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnimalSpawner.generated.h"

class AWildAnimal;
class UAnimalSpeciesData;

/** 開始時にナビメッシュ上へ群れを配置する。レベルに置いて使う */
UCLASS()
class HUNTINGGAME_API AAnimalSpawner : public AActor
{
	GENERATED_BODY()

public:
	AAnimalSpawner();

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<AWildAnimal> AnimalClass;

	/** 空ならクラスのデフォルトを使う */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	TObjectPtr<UAnimalSpeciesData> SpeciesOverride;

	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "1"))
	int32 MinHerdSize = 2;

	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "1"))
	int32 MaxHerdSize = 5;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	float SpawnRadius = 1500.f;

protected:
	virtual void BeginPlay() override;
};
