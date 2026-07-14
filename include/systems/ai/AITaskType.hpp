/**
 * @file AITaskType.hpp
 * @brief Defines all high-level AI task categories.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

enum class AITaskType {
    None,

    // Threat / survival
    Flee,
    Defend,
    SeekFood,
    Rest,

    // Family / village life
    CareChildFood,
    ReturnToVillageCore,

    // Equipment / requests
    EquipWeapon,
    RequestWeapon,
    FulfillWeaponRequest,

    // Logistics
    Store,
    Haul,

    // Profession work
    Guard,
    Repair,
    Patrol,
    Hunt,
    Build,
    Dismantle,
    Harvest,

    // Social
    Socialize,
    AvoidPerson,
    ConfrontPerson,

    // Religion
    Pray,
    Preach,
    HoldRitual,
    ComfortFrightened,

    // Hostility
    Intimidate,
    FightNonLethal,
    Murder,

    // Fallback
    Wander
};
