/**
 * @file AIPriority.hpp
 * @brief Centralized AI task priority constants.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

namespace AIPriority {

constexpr float Threat = 1000.0f;
constexpr float CriticalNeed = 900.0f;
constexpr float Guard = 860.0f;
constexpr float Care = 760.0f;

constexpr float EquipWeapon = 735.0f;
constexpr float RequestWeapon = 730.0f;
constexpr float FulfillRequest = 710.0f;

constexpr float HighNeed = 700.0f;
constexpr float AvoidPerson = 690.0f;

constexpr float ReturnToVillage = 610.0f;
constexpr float Rest = 520.0f;

constexpr float Logistics = 430.0f;
constexpr float Haul = 410.0f;

constexpr float Repair = 280.0f;
constexpr float Work = 250.0f;

constexpr float ComfortFrightened = 210.0f;
constexpr float Murder = 195.0f;
constexpr float Socialize = 190.0f;
constexpr float FightNonLethal = 185.0f;
constexpr float ConfrontPerson = 180.0f;
constexpr float Intimidate = 175.0f;

constexpr float Preach = 145.0f;
constexpr float HoldRitual = 140.0f;
constexpr float Patrol = 120.0f;
constexpr float Pray = 75.0f;

constexpr float Idle = 5.0f;

} // namespace AIPriority
