﻿#pragma once

// Food, potions, ammo, armor. Things that go in the utility slot.

namespace game
{
	void equipAmmoByForm(const RE::TESForm* a_form, RE::PlayerCharacter*& a_player);
	void unequipCurrentAmmo();

	// Equip this armor. Does nothing if it's already equipped.
	void equipArmorByForm(const RE::TESForm* form, RE::PlayerCharacter*& player);
	// Equip if unequipped, un-equip if equipped already.
	void toggleArmorByForm(const RE::TESForm* form, RE::PlayerCharacter*& player);
	// reurns true if anything was unequipped.
	bool unequipArmor(RE::TESBoundObject*& a_obj,
		RE::PlayerCharacter*& a_player,
		RE::ActorEquipManager*& a_actor_equip_manager);

	void consumePotion(const RE::TESForm* a_form, RE::PlayerCharacter*& a_player);
	void consumeBestOption(RE::ActorValue a_actor_value);
	void poison_weapon(RE::PlayerCharacter*& a_player, RE::AlchemyItem*& a_poison, uint32_t a_count);

	class perk_visitor : public RE::PerkEntryVisitor
	{
	public:
		explicit perk_visitor(RE::Actor* a_actor, float a_base)
		{
			actor_  = a_actor;
			result_ = a_base;
		}

		RE::BSContainer::ForEachResult Visit(RE::BGSPerkEntry* perk_entry) override;

		[[nodiscard]] float get_result() const;

	protected:
		RE::Actor* actor_;
		float result_;
	};
}
