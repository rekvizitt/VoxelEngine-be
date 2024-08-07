#include "ItemStack.hpp"

#include "ItemDef.hpp"
#include "../content/Content.hpp"

ItemStack::ItemStack() : item(ITEM_EMPTY), count(0) {
}

ItemStack::ItemStack(itemid_t item, itemcount_t count) : item(item), count(count) {
}

void ItemStack::set(const ItemStack& item) {
    this->item = item.item;
    this->count = item.count;
    if (count == 0) {
        this->item = 0;
    }
}

bool ItemStack::accepts(const ItemStack& other) const {
    if (isEmpty()) {
        return true;
    }
    return item == other.getItemId();
}

void ItemStack::move(ItemStack& item, const ContentIndices* indices) {
    auto def = indices->items.get(item.getItemId());
    int count = std::min(item.count, def->stackSize-this->count);
    if (isEmpty()) {
        set(ItemStack(item.getItemId(), count));
    } else {
        setCount(this->count + count);
    }
    item.setCount(item.count-count);
}

void ItemStack::setCount(itemcount_t count) {
    this->count = count;
    if (count == 0) {
        item = 0;
    }
}
