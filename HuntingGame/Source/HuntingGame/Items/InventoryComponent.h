#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Hunting), meta = (BlueprintSpawnableComponent))
class HUNTINGGAME_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItem(FName ItemId, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FName ItemId, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCount(FName ItemId) const;

	const TMap<FName, int32>& GetItems() const { return Items; }

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

private:
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TMap<FName, int32> Items;
};
