﻿#pragma once

namespace ui
{
	constexpr auto hud_name  = "lamas_tiny_hud";
	constexpr auto draw_full = 255;

	static std::string icon_directory                = R"(.\Data\SKSE\Plugins\resources\icons)";
	static std::string img_directory                 = R"(.\Data\SKSE\Plugins\resources\img)";
	static std::string highlight_animation_directory = R"(.\Data\SKSE\Plugins\resources\animation\highlight)";

	enum class image_type
	{
		hud,
		round,
		key,
		total
	};

	inline static std::map<std::string, image_type> image_type_name_map = { { R"(hud.svg)", image_type::hud },
		{ R"(round.svg)", image_type::round },
		{ R"(key.svg)", image_type::key } };

	enum class icon_image_type
	{
		potion_health,
		potion_default,
		sword_one_handed,
		axe_one_handed,
		mace,
		dagger,
		sword_two_handed,
		axe_two_handed,
		staff,
		bow,
		crossbow,
		spell_default,
		destruction,
		shout,
		power,
		food,
		shield,
		icon_default,
		destruction_fire,
		destruction_frost,
		destruction_shock,
		restoration,
		poison_default,
		armor_light,
		armor_heavy,
		armor_clothing,
		scroll,
		arrow,
		hand_to_hand,
		katana,
		halberd,
		whip,
		claw,
		rapier,
		quarter_staff,
		pike,
		potion_stamina,
		potion_magicka,
		potion_fire_resist,
		potion_shock_resist,
		potion_frost_resist,
		potion_magic_resist,
		alteration,
		conjuration,
		illusion,
		torch,
		lantern,
		mask,
		total
	};


	inline static std::map<std::string, icon_image_type> icon_type_name_map = { { R"(potion_health.svg)",
																					icon_image_type::potion_health },
		{ R"(potion_default.svg)", icon_image_type::potion_default },
		{ R"(sword_one_handed.svg)", icon_image_type::sword_one_handed },
		{ R"(axe_one_handed.svg)", icon_image_type::axe_one_handed },
		{ R"(mace.svg)", icon_image_type::mace },
		{ R"(dagger.svg)", icon_image_type::dagger },
		{ R"(sword_two_handed.svg)", icon_image_type::sword_two_handed },
		{ R"(axe_two_handed.svg)", icon_image_type::axe_two_handed },
		{ R"(staff.svg)", icon_image_type::staff },
		{ R"(bow.svg)", icon_image_type::bow },
		{ R"(crossbow.svg)", icon_image_type::crossbow },
		{ R"(spell_default.svg)", icon_image_type::spell_default },
		{ R"(destruction.svg)", icon_image_type::destruction },
		{ R"(shout.svg)", icon_image_type::shout },
		{ R"(power.svg)", icon_image_type::power },
		{ R"(food.svg)", icon_image_type::food },
		{ R"(shield.svg)", icon_image_type::shield },
		{ R"(icon_default.svg)", icon_image_type::icon_default },
		{ R"(destruction_fire.svg)", icon_image_type::destruction_fire },
		{ R"(destruction_frost.svg)", icon_image_type::destruction_frost },
		{ R"(destruction_shock.svg)", icon_image_type::destruction_shock },
		{ R"(restoration.svg)", icon_image_type::restoration },
		{ R"(poison_default.svg)", icon_image_type::poison_default },
		{ R"(armor_light.svg)", icon_image_type::armor_light },
		{ R"(armor_heavy.svg)", icon_image_type::armor_heavy },
		{ R"(armor_clothing.svg)", icon_image_type::armor_clothing },
		{ R"(scroll.svg)", icon_image_type::scroll },
		{ R"(arrow.svg)", icon_image_type::arrow },
		{ R"(hand_to_hand.svg)", icon_image_type::hand_to_hand },
		{ R"(katana.svg)", icon_image_type::katana },
		{ R"(halberd.svg)", icon_image_type::halberd },
		{ R"(whip.svg)", icon_image_type::whip },
		{ R"(claw.svg)", icon_image_type::claw },
		{ R"(rapier.svg)", icon_image_type::rapier },
		{ R"(quarter_staff.svg)", icon_image_type::quarter_staff },
		{ R"(pike.svg)", icon_image_type::pike },
		{ R"(potion_stamina.svg)", icon_image_type::potion_stamina },
		{ R"(potion_magicka.svg)", icon_image_type::potion_magicka },
		{ R"(potion_fire_resist.svg)", icon_image_type::potion_fire_resist },
		{ R"(potion_shock_resist.svg)", icon_image_type::potion_shock_resist },
		{ R"(potion_frost_resist.svg)", icon_image_type::potion_frost_resist },
		{ R"(potion_magic_resist.svg)", icon_image_type::potion_magic_resist },
		{ R"(alteration.svg)", icon_image_type::alteration },
		{ R"(conjuration.svg)", icon_image_type::conjuration },
		{ R"(illusion.svg)", icon_image_type::illusion },
		{ R"(torch.svg)", icon_image_type::torch },
		{ R"(lantern.svg)", icon_image_type::lantern },
		{ R"(mask.svg)", icon_image_type::mask } };
}
