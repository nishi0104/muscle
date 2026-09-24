#include "Items/InventoryComponent.h"

void UInventoryComponent::AddItem(FName ItemId, int32 Amount)
{
	if (ItemId.IsNone() || Amount <= 0)
	{
		return;
	}
	Items.FindOrAdd(ItemId) += Amount;
	OnInventoryChanged.Broadcast();
}

bool UInventoryComponent::RemoveItem(FName ItemId, int32 Amount)
{
	int32* Count = Items.Find(ItemId);
	if (!Count || *Count < Amount)
	{
		return false;
	}
	*Count -= Amount;
	if (*Count == 0)
	{
		Items.Remove(ItemId);
	}
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UInventoryComponent::GetCount(FName ItemId) const
{
	const int32* Count = Items.Find(ItemId);
	return Count ? *Count : 0;
}
