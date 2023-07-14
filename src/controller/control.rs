use std::collections::HashMap;
use std::sync::Mutex;

use once_cell::sync::Lazy;

use super::cycles::*;
use super::settings::user_settings;
use crate::plugin::*;

/// There can be only one. Not public because we want access managed.
// Does this really need to be a mutex? I think we're single-threaded...
static CONTROLLER: Lazy<Mutex<Controller>> = Lazy::new(|| Mutex::new(Controller::new()));

/// This mod bundles up the public-facing interface of the controller for ease
/// of import into the bridge. I do not want to give the C++ side this object.
pub mod public {
    use super::*;
    use crate::layout::*;

    /// C++ tells us when it's safe to start pulling together the data we need.
    pub fn initialize_hud() {
        let mut ctrl = CONTROLLER.lock().unwrap();
        let _settings = user_settings();
        let _layout = layout();

        // here we should validate all four cycle entries which might refer to now-missing items
        // player::has_item_or_spell(form) is the function to call

        // now walk through what we should be showing in each slot, whether in the cycle or not
        let _refresh = ctrl.update_equipped();
        // The readied utility item is purely in our control, so we can use whatever we have
        // top-of-cycle for that one.

        // TODO
    }

    /// Function for C++ to call to send a relevant button event to us.
    pub fn handle_key_event(key: u32, button: &ButtonEvent) -> KeyEventResponse {
        let action = Action::from(key);
        log::info!("incoming key event; key={key}; ");
        CONTROLLER.lock().unwrap().handle_key_event(action, button)
    }

    /// Function for C++ to call to send a relevant menu button-event to us.
    ///
    /// We get a fully-filled out CycleEntry struct to use as we see fit.
    pub fn handle_menu_event(key: u32, menu_item: Box<CycleEntry>) -> MenuEventResponse {
        let action = Action::from(key);
        CONTROLLER.lock().unwrap().toggle_item(action, *menu_item)
    }

    /// Get information about the item equipped in a specific slot.
    pub fn equipped_in_slot(element: HudElement) -> Box<CycleEntry> {
        CONTROLLER.lock().unwrap().equipped_in_slot(element)
    }

    // Handle an equip delay timer expiring.
    pub fn timer_expired(slot: Action) {
        // Fun time! We get to equip an item now!
        let ctrl = CONTROLLER.lock().unwrap();
        ctrl.timer_expired(slot);
    }

    /// Update our view of the player's equipment.
    pub fn update_equipped() -> bool {
        let mut ctrl = CONTROLLER.lock().unwrap();
        ctrl.update_equipped()
    }

    /// We know for sure the player just equipped this item.
    pub fn handle_item_equipped(form: &TESForm) -> bool {
        let mut ctrl = CONTROLLER.lock().unwrap();
        ctrl.handle_item_equipped(form)
    }

    /// A consumable's count changed. Record if relevant.
    pub fn handle_inventory_changed(form: &TESForm, count: u32) {
        let mut ctrl = CONTROLLER.lock().unwrap();
        ctrl.handle_inventory_changed(form, count);
    }
}

impl From<u32> for Action {
    /// Turn the key code into an enum for easier processing.
    fn from(value: u32) -> Self {
        let settings = user_settings();

        if value == settings.left {
            Action::Left
        } else if value == settings.right {
            Action::Right
        } else if value == settings.power {
            Action::Power
        } else if value == settings.utility {
            Action::Utility
        } else if value == settings.activate {
            Action::Activate
        } else if value == settings.showhide {
            Action::ShowHide
        } else if value == settings.refresh_layout {
            Action::RefreshLayout
        } else {
            Action::Irrelevant
        }
    }
}

/// What, model/view/controller? In my UI application? oh no
#[derive(Clone, Default, Debug)]
pub struct Controller {
    /// Our currently-active cycles.
    cycles: CycleData,
    // The items that the player has equipped right now.
    equipped: HashMap<HudElement, CycleEntry>,
    /// True if we've got a two-handed weapon equipped right now.
    two_hander_equipped: bool,
    /// The item that was in the left hand when we equipped a two-hander.
    left_hand_cached: Option<CycleEntry>,
}

impl Controller {
    /// Make a controller. Cycle data is read from disk. Currently-equipped
    /// items are not handled yet.
    pub fn new() -> Self {
        let cycles = CycleData::read().unwrap_or_default();
        Controller {
            cycles,
            ..Default::default()
        }
    }

    /// The player's inventory changed! Act on it if we need to.
    fn handle_inventory_changed(&mut self, _form: &TESForm, _count: u32) {
        // TODO handle consumable item count change
        todo!()
    }

    /// When the equip delay for a cycle expires, equip the item at the top.
    ///
    /// This function implements a critical function in the mod: equipping
    /// items. When the delay timer expires, we're notified to act on the
    /// player's changes to the cycle rotation. The delay exists to let the
    /// player tap a hotkey repeatedly to look at the items in a cycle without
    /// equipping each one of them as they go. Instead we wait for a little bit,
    /// and if we've had no more hotkey events, we act.
    ///
    /// We do not act here on cascading changes. Instead, we let the equipped-change
    /// callback decide what to do when, e.g., a two-handed item is equipped.
    fn timer_expired(&self, which: Action) {
        if matches!(which, Action::Left) && self.two_hander_equipped {
            // The left hand is blocked because the right hand is equipping a two-hander.
            // TODO honk
            return;
        }

        let Some(item) = &self.cycles.get_top(which) else {
            return;
        };

        let kind = item.kind();
        if matches!(kind, EntryKind::Empty) && which != Action::Utility {
            unequipSlot(which);
            return;
        }

        if matches!(which, Action::Power) {
            // Equip that fus-ro-dah, dovahkin!
            cxx::let_cxx_string!(form_spec = item.form_string());
            equipShout(&form_spec);
            return;
        }

        if matches!(which, Action::Right) && self.two_hander_equipped && !item.two_handed() {
            let cached_left = self.left_hand_cached.clone();
            self.equip_item(item, which);
            if let Some(left) = cached_left {
                self.equip_item(&left, Action::Left);
            }
            return;
        }

        self.equip_item(item, which);
    }

    /// Convenience function for equipping any equippable.
    fn equip_item(&self, item: &CycleEntry, which: Action) {
        if !matches!(which, Action::Right | Action::Left | Action::Utility) {
            return;
        }
        let kind = item.kind();
        cxx::let_cxx_string!(form_spec = item.form_string());

        // These are all different because the game API is a bit of an evolved thing.
        if kind.is_magic() {
            // My name is John Wellington Wells / I'm a dealer in...
            equipMagic(&form_spec, which);
        } else if kind.is_weapon() {
            equipWeapon(&form_spec, which);
        } else if kind.is_armor() {
            equipArmor(&form_spec);
        } else {
            log::info!(
                "we did nothing with item name={}; kind={kind:?};",
                item.name()
            );
        }
    }

    // TODO refs instead of cloning
    /// Get the item equipped in a specific slot. I'd like to return an option but I can't.
    fn equipped_in_slot(&self, slot: HudElement) -> Box<CycleEntry> {
        let Some(candidate) = self.equipped.get(&slot) else {
            return Box::<CycleEntry>::default();
        };

        Box::new(candidate.clone())
    }

    /// Returns true if our view of the player changed.
    fn update_equipped(&mut self) -> bool {
        let mut changed = false;

        let right_entry = equippedRightHand();
        log::info!(
            "right hand: {} {}",
            right_entry.name(),
            right_entry.form_string()
        );
        changed = changed || self.update_slot(HudElement::Right, &right_entry);

        let left_entry = if right_entry.two_handed() {
            self.two_hander_equipped = true;
            self.left_hand_cached = self.equipped.get(&HudElement::Left).cloned();
            // We show an empty entry in the left hand if we're holding a bow or other 2-hander
            Box::<CycleEntry>::default()
        } else {
            self.two_hander_equipped = false;
            if let Some(left) = &self.left_hand_cached.clone() {
                self.equip_item(left, Action::Left);
                self.left_hand_cached = None;
                Box::new(left.clone())
            } else {
                equippedLeftHand()
            }
        };
        log::info!(
            "left hand: {} {}",
            left_entry.name(),
            left_entry.form_string()
        );
        changed = changed || self.update_slot(HudElement::Left, &left_entry);

        let power = equippedPower();
        log::info!("power: {} {}", power.name(), power.form_string());
        changed = changed || self.update_slot(HudElement::Power, &power);

        let ammo = equippedAmmo();
        log::info!(
            "ammo: {} {} {}",
            ammo.count(),
            ammo.name(),
            ammo.form_string()
        );
        changed = changed || self.update_slot(HudElement::Ammo, &ammo);

        changed
    }

    fn handle_item_equipped(&mut self, _form: &TESForm) -> bool {
        // TODO implement a tighter pass; for now we just brute-force it
        // remember to mark if we've equipped a two-hander in the shorter impl
        self.update_equipped()
    }

    fn update_slot(&mut self, slot: HudElement, new_item: &CycleEntry) -> bool {
        if let Some(replaced) = self.equipped.insert(slot, new_item.clone()) {
            replaced != *new_item
        } else {
            false
        }
    }

    /// Handle a key-press event that the event system decided we need to know about.
    ///
    /// Returns an enum indicating what we did in response, in case one of the calling
    /// layers wants to show UI or play sounds in response.
    fn handle_key_event(&mut self, action: Action, button: &ButtonEvent) -> KeyEventResponse {
        // If we're faded out in any way, show ourselves again.
        if !matches!(action, Action::ShowHide) {
            let is_fading: bool = get_is_transitioning();
            if user_settings().fade() && !is_fading {
                set_alpha_transition(true, 1.0);
                return KeyEventResponse {
                    handled: true,
                    ..Default::default()
                };
            }
        }

        // TODO do I need this data?
        let _is_down: bool = button.IsDown();
        let _is_up: bool = button.IsUp();

        // TODO implement!
        match action {
            Action::Power => {
                let _next = self.cycles.advance(action, 1);
                KeyEventResponse {
                    handled: true,
                    start_timer: Action::Power,
                    stop_timer: Action::Irrelevant,
                }
            }
            Action::Left => {
                let _next = self.cycles.advance(action, 1);
                KeyEventResponse {
                    handled: true,
                    start_timer: Action::Left,
                    stop_timer: Action::Irrelevant,
                }
            }
            Action::Right => {
                let _next = self.cycles.advance(action, 1);
                KeyEventResponse {
                    handled: true,
                    start_timer: Action::Right,
                    stop_timer: Action::Irrelevant,
                }
            }
            Action::Utility => {
                let _next = self.cycles.advance(action, 1);
                KeyEventResponse {
                    handled: true,
                    start_timer: Action::Utility,
                    stop_timer: Action::Irrelevant,
                }
            }
            Action::Activate => self.use_utility_item(),
            Action::ShowHide => {
                log::debug!("toggling hud visibility");
                toggle_hud_visibility();
                KeyEventResponse {
                    handled: true,
                    ..Default::default()
                }
            }
            Action::RefreshLayout => {
                HudLayout::refresh();
                KeyEventResponse {
                    handled: true,
                    ..Default::default()
                }
            }
            _ => KeyEventResponse::default(),
        }
    }

    /// Activate whatever we have in the utility slot.
    fn use_utility_item(&mut self) -> KeyEventResponse {
        if let Some(item) = self.cycles.get_top(Action::Utility) {
            if item.kind() != EntryKind::Empty {
                // TODO use it
                // poison, potion, food, armor, lantern
            }
        }

        // No matter what we did, we stop the timer.
        KeyEventResponse {
            handled: true,
            start_timer: Action::Irrelevant,
            stop_timer: Action::Utility,
        }
    }

    /// This function is called when the player has pressed a hot key while hovering over an
    /// item in a menu. We'll remove the item if it's already in the matching cycle,
    /// or add it if it's an appropriate item. We signal back to the UI layer what we did.
    fn toggle_item(&mut self, action: Action, item: CycleEntry) -> MenuEventResponse {
        let result = self.cycles.toggle(action, item.clone());

        // notify the player what happened...
        let verb = match result {
            MenuEventResponse::ItemAdded => "added to",
            MenuEventResponse::ItemRemoved => "removed from",
            _ => "not changed in",
        };
        let cyclename = match action {
            Action::Power => "powers",
            Action::Left => "left-hand",
            Action::Right => "right-hand",
            Action::Utility => "utility items",
            _ => "any",
        };
        let message = format!("{} {} {} cycle", item.name(), verb, cyclename);
        cxx::let_cxx_string!(msg = message);
        notify_player(&msg);

        if matches!(
            result,
            MenuEventResponse::ItemAdded | MenuEventResponse::ItemRemoved
        ) {
            // the data changed. flush it to disk with char name in it or something
            match self.cycles.write() {
                Ok(_) => log::info!(
                    "persisted cycle data after change; verb='{}'; item='{}';",
                    verb,
                    item.name()
                ),
                Err(e) => {
                    log::warn!("failed to persist cycle data, but gamely continuing; {e:?}");
                }
            }
        }

        result
    }
}

impl Default for KeyEventResponse {
    fn default() -> Self {
        Self {
            handled: false,
            stop_timer: Action::Irrelevant,
            start_timer: Action::Irrelevant,
        }
    }
}
