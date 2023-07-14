﻿#include "magic.h"
#include "gear.h"
#include "offset.h"
#include "player.h"
#include "user_settings.h"

namespace magic
{
	//add toggle mcm if equip or cast
	void cast_magic(RE::TESForm* a_form,
		action_type a_action,
		const RE::BGSEquipSlot* a_slot,
		RE::PlayerCharacter*& a_player)
	{
		auto left = a_slot == equip::left_hand_equip_slot();
		logger::trace("try to work spell {}, action {}, left {}"sv,
			a_form->GetName(),
			static_cast<uint32_t>(a_action),
			left);

		if (!a_form->Is(RE::FormType::Spell))
		{
			logger::warn("object {} is not a spell. return."sv, a_form->GetName());
			return;
		}

		auto* spell = a_form->As<RE::SpellItem>();

		if (!a_player->HasSpell(spell))
		{
			logger::warn("player does not have spell {}. return."sv, spell->GetName());
			return;
		}

		//maybe check if the spell is already equipped
		auto casting_type = spell->GetCastingType();
		logger::trace("spell {} is type {}"sv, spell->GetName(), static_cast<uint32_t>(casting_type));
		if (a_action == action_type::instant && casting_type != RE::MagicSystem::CastingType::kConcentration)
		{
			if (config::mcm_setting::get_elden_demon_souls())
			{
				auto selected_power = a_player->GetActorRuntimeData().selectedPower;
				if (selected_power)
				{
					logger::warn(
						"power/shout {} is equipped, will only cast spell in elden mode if shout slot is empty. return."sv,
						selected_power->GetName());
					RE::DebugNotification("Shout Slot not Empty, Skipping Spellcast");
					return;
				}
			}
			auto* actor  = a_player->As<RE::Actor>();
			auto* caster = actor->GetMagicCaster(get_casting_source(a_slot));

			//might cost nothing if nothing has been equipped into tha hands after start, so it seems
			auto cost = spell->CalculateMagickaCost(actor);
			logger::trace("spell cost for {} is {}"sv, spell->GetName(), fmt::format(FMT_STRING("{:.2f}"), cost));

			auto current_magicka = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
			auto dual_cast       = false;
			if (!spell->IsTwoHanded() && config::mcm_setting::get_try_dual_cast_top_spell() &&
				config::mcm_setting::get_elden_demon_souls())
			{
				auto* game_setting             = RE::GameSettingCollection::GetSingleton();
				auto dual_cast_cost_multiplier = game_setting->GetSetting("fMagicDualCastingCostMult")->GetFloat();
				logger::trace("dual cast, multiplier {}"sv,
					fmt::format(FMT_STRING("{:.2f}"), dual_cast_cost_multiplier));
				dual_cast = can_dual_cast(cost, current_magicka, dual_cast_cost_multiplier);
				if (dual_cast)
				{
					cost = cost * dual_cast_cost_multiplier;
					caster->SetDualCasting(true);
				}
			}
			logger::trace("got temp magicka {}, cost {}, can dual cast {}"sv, current_magicka, cost, dual_cast);

			if (current_magicka < cost)
			{
				if (!RE::UI::GetSingleton()->GetMenu<RE::HUDMenu>())
				{
					logger::warn("Will not flash HUD Menu, because I could not find it.");
				}
				else
				{
					flash_hud_meter(RE::ActorValue::kMagicka);
				}
				logger::warn("not enough magicka for spell {}, magicka {}, cost {} return."sv,
					a_form->GetName(),
					current_magicka,
					cost);
				return;
			}

			actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
				RE::ActorValue::kMagicka,
				-cost);

			//could trigger an animation here
			//might need to set some things
			//TODO make an animation to play here
			//a_player->NotifyAnimationGraph("IdleMagic_01"); //works
			auto is_self_target = spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf;
			auto* target        = is_self_target ? actor : actor->GetActorRuntimeData().currentCombatTarget.get().get();

			auto magnitude     = 1.f;
			auto effectiveness = 1.f;
			if (auto* effect = spell->GetCostliestEffectItem())
			{
				magnitude = effect->GetMagnitude();
			}
			logger::trace("casting spell {}, magnitude {}, effectiveness {}"sv,
				spell->GetName(),
				fmt::format(FMT_STRING("{:.2f}"), magnitude),
				fmt::format(FMT_STRING("{:.2f}"), effectiveness));
			caster->CastSpellImmediate(spell,
				false,
				target,
				effectiveness,
				false,
				magnitude,
				is_self_target ? nullptr : actor);
			//tested with adamant, works with the silent casting perk as well
			send_spell_casting_sound_alert(caster, spell);
		}
		else
		{
			const auto* obj_right = a_player->GetActorRuntimeData().currentProcess->GetEquippedRightHand();
			const auto* obj_left  = a_player->GetActorRuntimeData().currentProcess->GetequippedLeftHand();
			if (left && obj_left && obj_left->formID == spell->formID)
			{
				logger::debug("Object Left {} is already where it should be already equipped. return."sv,
					spell->GetName());
				return;
			}
			if (!left && obj_right && obj_right->formID == spell->formID)
			{
				logger::debug("Object Right {} is already where it should be already equipped. return."sv,
					spell->GetName());
				return;
			}

			logger::trace("calling equip spell {}, left {}"sv, spell->GetName(), left);
			auto* task = SKSE::GetTaskInterface();
			if (task)
			{
				task->AddTask([=]() { RE::ActorEquipManager::GetSingleton()->EquipSpell(a_player, spell, a_slot); });
			}
		}

		logger::trace("worked spell {}, action {}. return."sv, a_form->GetName(), static_cast<uint32_t>(a_action));
	}

	void cast_scroll(const RE::TESForm* a_form, action_type a_action, RE::PlayerCharacter*& a_player)
	{
		logger::trace("try to work scroll {}, action {}"sv, a_form->GetName(), static_cast<uint32_t>(a_action));

		if (!a_form->Is(RE::FormType::Scroll))
		{
			logger::warn("object {} is not a scroll. return."sv, a_form->GetName());
			return;
		}

		RE::TESBoundObject* obj = nullptr;
		auto left               = 0;
		for (auto potential_items = player::get_inventory(a_player, RE::FormType::Scroll);
			 const auto& [item, inv_data] : potential_items)
		{
			if (const auto& [num_items, entry] = inv_data; entry->object->formID == a_form->formID)
			{
				obj  = item;
				left = num_items;
				break;
			}
		}

		if (!obj || left == 0)
		{
			logger::warn("could not find selected scroll, maybe it all have been consumed"sv);
			return;
		}

		if (a_action == action_type::instant)
		{
			auto* actor  = a_player->As<RE::Actor>();
			auto* scroll = obj->As<RE::ScrollItem>();
			actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)
				->CastSpellImmediate(scroll, false, actor, 1.0f, false, 0.0f, nullptr);
			actor->RemoveItem(scroll, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		}
		else
		{
			auto* task = SKSE::GetTaskInterface();
			if (task)
			{
				task->AddTask([=]() { RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, obj); });
			}
		}

		logger::trace("worked scroll {}, action {}. return."sv, a_form->GetName(), static_cast<uint32_t>(a_action));
	}

	void equip_or_cast_power(RE::TESForm* a_form, action_type a_action, RE::PlayerCharacter*& a_player)
	{
		logger::trace("try to work power {}, action {}"sv, a_form->GetName(), static_cast<uint32_t>(a_action));

		if (!a_form->Is(RE::FormType::Spell))
		{
			logger::warn("object {} is not a spell. return."sv, a_form->GetName());
			return;
		}

		if (const auto* selected_power = a_player->GetActorRuntimeData().selectedPower;
			selected_power && a_action != enums::action_type::instant)
		{
			logger::trace("current selected power is {}, is shout {}, is spell {}"sv,
				selected_power->GetName(),
				selected_power->Is(RE::FormType::Shout),
				selected_power->Is(RE::FormType::Spell));
			if (selected_power->formID == a_form->formID)
			{
				logger::debug("no need to equip power {}, it is already equipped. return."sv, a_form->GetName());
				return;
			}
		}

		auto* spell = a_form->As<RE::SpellItem>();
		if (!a_player->HasSpell(spell))
		{
			logger::warn("player does not have spell {}. return."sv, spell->GetName());
			return;
		}


		RE::ActorEquipManager::GetSingleton()->EquipSpell(a_player, spell);
		logger::trace("worked power {} action {}. return."sv, a_form->GetName(), static_cast<uint32_t>(a_action));
	}

	void equipShout(RE::TESForm* a_form, RE::PlayerCharacter*& a_player)
	{
		logger::trace("try to equip shout {}"sv, a_form->GetName());

		if (!a_form->Is(RE::FormType::Shout))
		{
			logger::warn("object {} is not a shout. return."sv, a_form->GetName());
			return;
		}

		if (const auto selected_power = a_player->GetActorRuntimeData().selectedPower; selected_power)
		{
			logger::trace("current selected power is {}, is shout {}, is spell {}"sv,
				selected_power->GetName(),
				selected_power->Is(RE::FormType::Shout),
				selected_power->Is(RE::FormType::Spell));
			if (selected_power->formID == a_form->formID)
			{
				logger::debug("no need to equip shout {}, it is already equipped. return."sv, a_form->GetName());
				return;
			}
		}

		auto* shout = a_form->As<RE::TESShout>();
		if (!player::has_shout(a_player, shout))
		{
			logger::warn("player does not have spell {}. return."sv, shout->GetName());
			return;
		}

		RE::ActorEquipManager::GetSingleton()->EquipShout(a_player, shout);
		logger::trace("equipped shout {}. return."sv, a_form->GetName());
	}

	RE::MagicSystem::CastingSource get_casting_source(const RE::BGSEquipSlot* a_slot)
	{
		if (a_slot == equip::right_hand_equip_slot())
		{
			return RE::MagicSystem::CastingSource::kRightHand;
		}
		if (a_slot == equip::left_hand_equip_slot())
		{
			return RE::MagicSystem::CastingSource::kLeftHand;
		}
		return RE::MagicSystem::CastingSource::kOther;
	}

	bool can_dual_cast(float a_cost, float a_magicka, float a_multiplier)
	{
		if ((a_cost * a_multiplier) < a_magicka)
		{
			return true;
		}
		return false;
	}

	void flash_hud_meter(RE::ActorValue a_actor_value)
	{
		static REL::Relocation<decltype(flash_hud_meter)> flash_hud_meter{ REL::ID(offset::get_flash_hud_meter) };
		return flash_hud_meter(a_actor_value);
	}

	void send_spell_casting_sound_alert(RE::MagicCaster* a_magic_caster, RE::SpellItem* a_spell_item)
	{
		static REL::Relocation<decltype(send_spell_casting_sound_alert)> send_spell_casting_sound_alert{ REL::ID(
			offset::send_spell_casting_sound_alert) };
		return send_spell_casting_sound_alert(a_magic_caster, a_spell_item);
	}

	// ---------- Spells.

	void unequip_spell(RE::BSScript::IVirtualMachine* a_vm,
		RE::VMStackID a_stack_id,
		RE::Actor* a_actor,
		RE::SpellItem* a_spell,
		uint32_t slot)
	{
		using func_t = decltype(&unequip_spell);
		const REL::Relocation<func_t> func{ REL::ID(offset::get_un_equip_spell) };
		func(a_vm, a_stack_id, a_actor, a_spell, slot);
	}

	// ---------- Shouts.

	void unequipShout(RE::BSScript::IVirtualMachine* a_vm,
		RE::VMStackID a_stack_id,
		RE::Actor* a_actor,
		RE::TESShout* a_shout)
	{
		using func_t = decltype(&unequip_shout);
		const REL::Relocation<func_t> func{ REL::ID(offset::get_un_equip_shout) };
		func(a_vm, a_stack_id, a_actor, a_shout);
	}

	void unequipShoutSlot(RE::PlayerCharacter*& player)
	{
		auto* selected_power = player->GetActorRuntimeData().selectedPower;
		if (selected_power)
		{
			logger::trace("Equipped form is {}, try to un equip"sv,
				util::string_util::int_to_hex(selected_power->formID));
			if (selected_power->Is(RE::FormType::Shout))
			{
				unequipShout(nullptr, 0, player, selected_power->As<RE::TESShout>());
			}
			else if (selected_power->Is(RE::FormType::Spell))
			{
				//power
				//2=other
				unequip_spell(nullptr, 0, player, selected_power->As<RE::SpellItem>(), 2);
			}
		}
	}

}
