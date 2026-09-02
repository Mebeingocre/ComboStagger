#define NOMINMAX
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include "PCH.h"
#include "SKSEMenuFramework.h"
#include "TrueHUDAPI.h"
#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <string>
#include <algorithm>
#include <cmath>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace ImGui = ImGuiMCP;
using ImVec4 = ImGuiMCP::ImVec4;

static TRUEHUD_API::IVTrueHUD3* g_trueHUD = nullptr;
static SKSE::PluginHandle g_pluginHandle = SKSE::kInvalidPluginHandle;

enum class ArmorWeightClass {
    kNone = 0,
    kLight = 1,
    kHeavy = 2
};

enum class CreatureTier {
    kPreyAndSmall = 0,
    kMedium = 1,
    kHeavyBeast = 2,
    kColossalBoss = 3
};

struct StaggerProfile {
    int hitBuffer = 0;
    int maxStaggers = 3;
    int cooldownSeconds = 10;
    float staggerMagnitude = 0.5f;

    void Save(const wchar_t* iniPath, const std::wstring& section) const {
        WritePrivateProfileStringW(section.c_str(), L"uHitBuffer", std::to_wstring(hitBuffer).c_str(), iniPath);
        WritePrivateProfileStringW(section.c_str(), L"uMaxStaggers", std::to_wstring(maxStaggers).c_str(), iniPath);
        WritePrivateProfileStringW(section.c_str(), L"uCooldownSeconds", std::to_wstring(cooldownSeconds).c_str(), iniPath);
        WritePrivateProfileStringW(section.c_str(), L"fStaggerMagnitude", std::to_wstring(staggerMagnitude).c_str(), iniPath);
    }

    void Load(const wchar_t* iniPath, const std::wstring& section, int defBuffer, int defMaxStags, int defCd, float defMag) {
        hitBuffer = GetPrivateProfileIntW(section.c_str(), L"uHitBuffer", defBuffer, iniPath);
        maxStaggers = GetPrivateProfileIntW(section.c_str(), L"uMaxStaggers", defMaxStags, iniPath);
        cooldownSeconds = GetPrivateProfileIntW(section.c_str(), L"uCooldownSeconds", defCd, iniPath);

        wchar_t magBuffer[32];
        GetPrivateProfileStringW(section.c_str(), L"fStaggerMagnitude", std::to_wstring(defMag).c_str(), magBuffer, 32, iniPath);
        staggerMagnitude = std::wcstof(magBuffer, nullptr);
    }
};

struct CreatureAttackMultipliers {
    float prey = 1.00f;
    float medium = 1.50f;
    float heavy = 3.50f;
    float colossal = 5.00f;

    void Save(const wchar_t* iniPath) const {
        const wchar_t* sec = L"Creature_Outgoing_Damage";
        auto writeF = [&](const wchar_t* key, float val) {
            WritePrivateProfileStringW(sec, key, std::to_wstring(val).c_str(), iniPath);
        };
        writeF(L"fPreyAndSmall", prey);
        writeF(L"fMediumMonsters", medium);
        writeF(L"fHeavyBeasts", heavy);
        writeF(L"fColossalBosses", colossal);
    }

    void Load(const wchar_t* iniPath) {
        const wchar_t* sec = L"Creature_Outgoing_Damage";
        auto readF = [&](const wchar_t* key, float defVal) -> float {
            wchar_t buf[32];
            GetPrivateProfileStringW(sec, key, std::to_wstring(defVal).c_str(), buf, 32, iniPath);
            return std::wcstof(buf, nullptr);
        };
        prey = readF(L"fPreyAndSmall", 1.00f);
        medium = readF(L"fMediumMonsters", 1.50f);
        heavy = readF(L"fHeavyBeasts", 3.50f);
        colossal = readF(L"fColossalBosses", 5.00f);
    }
};

struct WeaponMultipliers {
    float dagger = 0.75f;
    float sword = 1.30f;
    float warAxe = 1.40f;
    float mace = 1.50f;
    float greatsword = 2.10f;
    float battleaxe = 2.20f;
    float warhammer = 2.30f;
    float bow = 1.25f;
    float crossbow = 1.25f;
    float unarmed = 0.50f;
    float magic = 0.50f;
    float bashMult = 0.25f;
    float powerBashMult = 1.00f;
    int spellHitCooldownMs = 500;
    float powerAttackMult = 2.00f;
    float sneakAttackMult = 1.50f;

    void Save(const wchar_t* iniPath) const {
        const wchar_t* sec = L"WeaponMultipliers";
        auto writeF = [&](const wchar_t* key, float val) {
            WritePrivateProfileStringW(sec, key, std::to_wstring(val).c_str(), iniPath);
        };
        writeF(L"fDagger", dagger);
        writeF(L"fSword", sword);
        writeF(L"fWarAxe", warAxe);
        writeF(L"fMace", mace);
        writeF(L"fGreatsword", greatsword);
        writeF(L"fBattleaxe", battleaxe);
        writeF(L"fWarhammer", warhammer);
        writeF(L"fBow", bow);
        writeF(L"fCrossbow", crossbow);
        writeF(L"fUnarmed", unarmed);
        writeF(L"fMagic", magic);
        writeF(L"fBashMult", bashMult);
        writeF(L"fPowerBashMult", powerBashMult);
        WritePrivateProfileStringW(sec, L"uSpellHitCooldownMs", std::to_wstring(spellHitCooldownMs).c_str(), iniPath);
        writeF(L"fPowerAttackMult", powerAttackMult);
        writeF(L"fSneakAttackMult", sneakAttackMult);
    }

    void Load(const wchar_t* iniPath) {
        const wchar_t* sec = L"WeaponMultipliers";
        auto readF = [&](const wchar_t* key, float defVal) -> float {
            wchar_t buf[32];
            GetPrivateProfileStringW(sec, key, std::to_wstring(defVal).c_str(), buf, 32, iniPath);
            return std::wcstof(buf, nullptr);
        };
        dagger = readF(L"fDagger", 0.75f);
        sword = readF(L"fSword", 1.30f);
        warAxe = readF(L"fWarAxe", 1.40f);
        mace = readF(L"fMace", 1.50f);
        greatsword = readF(L"fGreatsword", 2.10f);
        battleaxe = readF(L"fBattleaxe", 2.20f);
        warhammer = readF(L"fWarhammer", 2.30f);
        bow = readF(L"fBow", 1.25f);
        crossbow = readF(L"fCrossbow", 1.25f);
        unarmed = readF(L"fUnarmed", 0.50f);
        magic = readF(L"fMagic", 0.50f);
        bashMult = readF(L"fBashMult", 0.25f);
        powerBashMult = readF(L"fPowerBashMult", 1.00f);
        spellHitCooldownMs = GetPrivateProfileIntW(sec, L"uSpellHitCooldownMs", 500, iniPath);
        powerAttackMult = readF(L"fPowerAttackMult", 2.00f);
        sneakAttackMult = readF(L"fSneakAttackMult", 1.50f);
    }
};

struct Settings {
    static Settings* GetSingleton() {
        static Settings instance;
        return &instance;
    }

    bool enabled = true;
    bool enableTrueHUD = true;
    float blockMitigationPercent = 0.80f;

    // Player Settings
    bool playerEnabled = true;
    int playerComboDecaySeconds = 15;
    float playerInstantBreakThreshold = 0.50f;
    StaggerProfile playerNone;
    StaggerProfile playerLight;
    StaggerProfile playerHeavy;

    // Humanoid NPC Settings
    bool npcEnabled = true;
    int npcComboDecaySeconds = 15;
    float npcInstantBreakThreshold = 0.50f;
    StaggerProfile npcNone;
    StaggerProfile npcLight;
    StaggerProfile npcHeavy;

    // Creature Settings
    bool creatureEnabled = true;
    float creatureInstantBreakThreshold = 0.50f;
    int creaturePreyDecaySeconds = 8;
    int creatureMediumDecaySeconds = 12;
    int creatureHeavyDecaySeconds = 15;
    int creatureColossalDecaySeconds = 20;
    StaggerProfile creaturePrey;
    StaggerProfile creatureMedium;
    StaggerProfile creatureHeavy;
    StaggerProfile creatureColossal;
    CreatureAttackMultipliers creatureAttacks;

    // Weapon Multipliers
    WeaponMultipliers multipliers;

    void ResetToDefaults() {
        enabled = true;
        enableTrueHUD = true;
        blockMitigationPercent = 0.80f;

        playerEnabled = true;
        playerComboDecaySeconds = 15;
        playerInstantBreakThreshold = 0.50f;
        playerNone = { 3, 6, 10, 0.60f };
        playerLight = { 6, 5, 12, 0.50f };
        playerHeavy = { 8, 4, 12, 0.25f };

        npcEnabled = true;
        npcComboDecaySeconds = 15;
        npcInstantBreakThreshold = 0.50f;
        npcNone = { 3, 7, 10, 1.00f };
        npcLight = { 6, 6, 12, 1.00f };
        npcHeavy = { 8, 5, 21, 1.00f };

        creatureEnabled = true;
        creatureInstantBreakThreshold = 0.50f;
        creaturePreyDecaySeconds = 8;
        creatureMediumDecaySeconds = 12;
        creatureHeavyDecaySeconds = 15;
        creatureColossalDecaySeconds = 20;

        creaturePrey = { 3, 3, 8, 0.75f };
        creatureMedium = { 7, 5, 12, 0.55f };
        creatureHeavy = { 10, 5, 12, 0.35f };
        creatureColossal = { 16, 8, 18, 0.10f };

        creatureAttacks.prey = 1.00f;
        creatureAttacks.medium = 1.50f;
        creatureAttacks.heavy = 3.50f;
        creatureAttacks.colossal = 5.00f;

        multipliers.dagger = 0.75f;
        multipliers.sword = 1.30f;
        multipliers.warAxe = 1.40f;
        multipliers.mace = 1.50f;
        multipliers.greatsword = 2.10f;
        multipliers.battleaxe = 2.20f;
        multipliers.warhammer = 2.30f;
        multipliers.bow = 1.25f;
        multipliers.crossbow = 1.25f;
        multipliers.unarmed = 0.50f;
        multipliers.magic = 0.50f;
        multipliers.bashMult = 0.25f;
        multipliers.powerBashMult = 1.00f;
        multipliers.spellHitCooldownMs = 500;
        multipliers.powerAttackMult = 2.00f;
        multipliers.sneakAttackMult = 1.50f;
    }

    void Save() const {
        const wchar_t* iniPath = L".\\Data\\SKSE\\Plugins\\ComboStagger.ini";

        WritePrivateProfileStringW(L"General", L"bEnabled", enabled ? L"1" : L"0", iniPath);
        WritePrivateProfileStringW(L"General", L"bEnableTrueHUD", enableTrueHUD ? L"1" : L"0", iniPath);
        WritePrivateProfileStringW(L"General", L"fBlockMitigationPercent", std::to_wstring(blockMitigationPercent).c_str(), iniPath);

        WritePrivateProfileStringW(L"Player_General", L"bEnabled", playerEnabled ? L"1" : L"0", iniPath);
        WritePrivateProfileStringW(L"Player_General", L"fInstantBreakThreshold", std::to_wstring(playerInstantBreakThreshold).c_str(), iniPath);
        WritePrivateProfileStringW(L"Player_General", L"uComboDecaySeconds", std::to_wstring(playerComboDecaySeconds).c_str(), iniPath);
        playerNone.Save(iniPath, L"Player_Unarmored");
        playerLight.Save(iniPath, L"Player_LightArmor");
        playerHeavy.Save(iniPath, L"Player_HeavyArmor");

        WritePrivateProfileStringW(L"NPC_General", L"bEnabled", npcEnabled ? L"1" : L"0", iniPath);
        WritePrivateProfileStringW(L"NPC_General", L"fInstantBreakThreshold", std::to_wstring(npcInstantBreakThreshold).c_str(), iniPath);
        WritePrivateProfileStringW(L"NPC_General", L"uComboDecaySeconds", std::to_wstring(npcComboDecaySeconds).c_str(), iniPath);
        npcNone.Save(iniPath, L"NPC_Unarmored");
        npcLight.Save(iniPath, L"NPC_LightArmor");
        npcHeavy.Save(iniPath, L"NPC_HeavyArmor");

        WritePrivateProfileStringW(L"Creatures_General", L"bEnabled", creatureEnabled ? L"1" : L"0", iniPath);
        WritePrivateProfileStringW(L"Creatures_General", L"fInstantBreakThreshold", std::to_wstring(creatureInstantBreakThreshold).c_str(), iniPath);
        WritePrivateProfileStringW(L"Creatures_General", L"uPreyDecaySeconds", std::to_wstring(creaturePreyDecaySeconds).c_str(), iniPath);
        WritePrivateProfileStringW(L"Creatures_General", L"uMediumDecaySeconds", std::to_wstring(creatureMediumDecaySeconds).c_str(), iniPath);
        WritePrivateProfileStringW(L"Creatures_General", L"uHeavyDecaySeconds", std::to_wstring(creatureHeavyDecaySeconds).c_str(), iniPath);
        WritePrivateProfileStringW(L"Creatures_General", L"uColossalDecaySeconds", std::to_wstring(creatureColossalDecaySeconds).c_str(), iniPath);

        creaturePrey.Save(iniPath, L"Creatures_PreyAndSmall");
        creatureMedium.Save(iniPath, L"Creatures_MediumMonsters");
        creatureHeavy.Save(iniPath, L"Creatures_HeavyBeasts");
        creatureColossal.Save(iniPath, L"Creatures_ColossalBosses");
        creatureAttacks.Save(iniPath);

        multipliers.Save(iniPath);
    }

    void Load() {
        const wchar_t* iniPath = L".\\Data\\SKSE\\Plugins\\ComboStagger.ini";

        enabled = GetPrivateProfileIntW(L"General", L"bEnabled", 1, iniPath) != 0;
        enableTrueHUD = GetPrivateProfileIntW(L"General", L"bEnableTrueHUD", 1, iniPath) != 0;

        wchar_t blkBuf[32];
        GetPrivateProfileStringW(L"General", L"fBlockMitigationPercent", L"0.800000", blkBuf, 32, iniPath);
        blockMitigationPercent = std::wcstof(blkBuf, nullptr);

        playerEnabled = GetPrivateProfileIntW(L"Player_General", L"bEnabled", 1, iniPath) != 0;
        wchar_t pThreshBuf[32];
        GetPrivateProfileStringW(L"Player_General", L"fInstantBreakThreshold", L"0.500000", pThreshBuf, 32, iniPath);
        playerInstantBreakThreshold = std::wcstof(pThreshBuf, nullptr);
        playerComboDecaySeconds = GetPrivateProfileIntW(L"Player_General", L"uComboDecaySeconds", 15, iniPath);

        playerNone.Load(iniPath, L"Player_Unarmored", 3, 6, 10, 0.60f);
        playerLight.Load(iniPath, L"Player_LightArmor", 6, 5, 12, 0.50f);
        playerHeavy.Load(iniPath, L"Player_HeavyArmor", 8, 4, 12, 0.25f);

        npcEnabled = GetPrivateProfileIntW(L"NPC_General", L"bEnabled", 1, iniPath) != 0;
        wchar_t npcThreshBuf[32];
        GetPrivateProfileStringW(L"NPC_General", L"fInstantBreakThreshold", L"0.500000", npcThreshBuf, 32, iniPath);
        npcInstantBreakThreshold = std::wcstof(npcThreshBuf, nullptr);
        npcComboDecaySeconds = GetPrivateProfileIntW(L"NPC_General", L"uComboDecaySeconds", 15, iniPath);

        npcNone.Load(iniPath, L"NPC_Unarmored", 3, 7, 10, 1.00f);
        npcLight.Load(iniPath, L"NPC_LightArmor", 6, 6, 12, 1.00f);
        npcHeavy.Load(iniPath, L"NPC_HeavyArmor", 8, 5, 21, 1.00f);

        creatureEnabled = GetPrivateProfileIntW(L"Creatures_General", L"bEnabled", 1, iniPath) != 0;
        wchar_t cThreshBuf[32];
        GetPrivateProfileStringW(L"Creatures_General", L"fInstantBreakThreshold", L"0.500000", cThreshBuf, 32, iniPath);
        creatureInstantBreakThreshold = std::wcstof(cThreshBuf, nullptr);

        creaturePreyDecaySeconds = GetPrivateProfileIntW(L"Creatures_General", L"uPreyDecaySeconds", 8, iniPath);
        creatureMediumDecaySeconds = GetPrivateProfileIntW(L"Creatures_General", L"uMediumDecaySeconds", 12, iniPath);
        creatureHeavyDecaySeconds = GetPrivateProfileIntW(L"Creatures_General", L"uHeavyDecaySeconds", 15, iniPath);
        creatureColossalDecaySeconds = GetPrivateProfileIntW(L"Creatures_General", L"uColossalDecaySeconds", 20, iniPath);

        creaturePrey.Load(iniPath, L"Creatures_PreyAndSmall", 3, 3, 8, 0.75f);
        creatureMedium.Load(iniPath, L"Creatures_MediumMonsters", 7, 5, 12, 0.55f);
        creatureHeavy.Load(iniPath, L"Creatures_HeavyBeasts", 10, 5, 12, 0.35f);
        creatureColossal.Load(iniPath, L"Creatures_ColossalBosses", 16, 8, 18, 0.10f);
        creatureAttacks.Load(iniPath);

        multipliers.Load(iniPath);
    }
};

inline RE::FormID GetCanonicalFormID(RE::Actor* a_actor) {
    if (!a_actor) return 0;
    if (a_actor->IsPlayerRef() || (a_actor == RE::PlayerCharacter::GetSingleton())) {
        return 0x14;
    }
    return a_actor->GetFormID();
}

bool HasKeywordOrName(RE::Actor* actor, const std::string& pattern) {
    if (!actor) return false;

    if (actor->HasKeywordString(pattern)) return true;

    auto* race = actor->GetRace();
    auto* base = actor->GetActorBase();

    if (race && race->HasKeywordString(pattern)) return true;
    if (base && base->HasKeywordString(pattern)) return true;

    std::string lowerPat = pattern;
    std::transform(lowerPat.begin(), lowerPat.end(), lowerPat.begin(), ::tolower);

    if (race) {
        std::string raceID = race->GetFormEditorID();
        std::transform(raceID.begin(), raceID.end(), raceID.begin(), ::tolower);
        if (raceID.find(lowerPat) != std::string::npos) return true;
    }

    if (base) {
        std::string baseName = base->GetName();
        std::transform(baseName.begin(), baseName.end(), baseName.begin(), ::tolower);
        if (baseName.find(lowerPat) != std::string::npos) return true;
    }

    const char* displayName = actor->GetDisplayFullName();
    if (displayName && displayName[0] != '\0') {
        std::string dispStr = displayName;
        std::transform(dispStr.begin(), dispStr.end(), dispStr.begin(), ::tolower);
        if (dispStr.find(lowerPat) != std::string::npos) return true;
    }

    return false;
}

bool IsBeastForm(RE::Actor* actor) {
    if (!actor) return false;
    return HasKeywordOrName(actor, "Werewolf") || 
           HasKeywordOrName(actor, "Werebear") || 
           HasKeywordOrName(actor, "VampireLord");
}

bool IsCreature(RE::Actor* actor) {
    if (!actor) return false;
    auto* race = actor->GetRace();
    if (!race) return false;

    if (race->HasKeywordString("ActorTypeNPC") || actor->HasKeywordString("ActorTypeNPC")) {
        return false;
    }
    if (race->data.flags.all(RE::RACE_DATA::Flag::kPlayable)) {
        return false;
    }

    if (race->HasKeywordString("ActorTypeCreature") ||
        race->HasKeywordString("ActorTypeAnimal") ||
        race->HasKeywordString("ActorTypeMonster") ||
        race->HasKeywordString("ActorTypeDragon") ||
        race->HasKeywordString("ActorTypeDwarven") ||
        race->HasKeywordString("ActorTypeGiant") ||
        race->HasKeywordString("ActorTypeTroll") ||
        race->HasKeywordString("ActorTypeUndead") ||
        race->HasKeywordString("ActorTypeDaedra")) {
        return true;
    }

    bool isChild = race->data.flags.all(RE::RACE_DATA::Flag::kChild);
    return !isChild;
}

CreatureTier GetCreatureTier(RE::Actor* actor) {
    if (!actor) return CreatureTier::kMedium;

    if (HasKeywordOrName(actor, "Dragon") ||
        HasKeywordOrName(actor, "Giant") ||
        HasKeywordOrName(actor, "Centurion") ||
        HasKeywordOrName(actor, "Mammoth") ||
        HasKeywordOrName(actor, "Gargoyle") ||
        HasKeywordOrName(actor, "Lurker") ||
        HasKeywordOrName(actor, "Karstaag") ||
        HasKeywordOrName(actor, "Ballista") ||
        HasKeywordOrName(actor, "AshGuardian") ||
        HasKeywordOrName(actor, "Keeper") ||
        HasKeywordOrName(actor, "Reaper") ||
        HasKeywordOrName(actor, "Seeker")) {
        return CreatureTier::kColossalBoss;
    }

    if (HasKeywordOrName(actor, "Troll") ||
        HasKeywordOrName(actor, "Bear") ||
        HasKeywordOrName(actor, "Sabre") ||
        HasKeywordOrName(actor, "Sabrecat") ||
        HasKeywordOrName(actor, "Werewolf") ||
        HasKeywordOrName(actor, "Werebear") ||
        HasKeywordOrName(actor, "VampireLord") ||
        HasKeywordOrName(actor, "Sphere") ||
        HasKeywordOrName(actor, "DragonPriest") ||
        HasKeywordOrName(actor, "Spriggan") ||
        HasKeywordOrName(actor, "Chaurus") ||
        HasKeywordOrName(actor, "Bristleback") ||
        HasKeywordOrName(actor, "Deathlord") ||
        HasKeywordOrName(actor, "Overlord") ||
        HasKeywordOrName(actor, "HulkingDraugr") ||
        HasKeywordOrName(actor, "Hulking Draugr")) {
        return CreatureTier::kHeavyBeast;
    }

    if (HasKeywordOrName(actor, "Deer") ||
        HasKeywordOrName(actor, "Elk") ||
        HasKeywordOrName(actor, "Fox") ||
        HasKeywordOrName(actor, "Skeever") ||
        HasKeywordOrName(actor, "Hare") ||
        HasKeywordOrName(actor, "Rabbit") ||
        HasKeywordOrName(actor, "Mudcrab") ||
        HasKeywordOrName(actor, "Goat") ||
        HasKeywordOrName(actor, "Dog") ||
        HasKeywordOrName(actor, "Husky") ||
        HasKeywordOrName(actor, "Chicken") ||
        HasKeywordOrName(actor, "Slaughterfish") ||
        HasKeywordOrName(actor, "DwarvenSpider") ||
        HasKeywordOrName(actor, "SpiderCenturion") ||
        HasKeywordOrName(actor, "Riekling") ||
        HasKeywordOrName(actor, "AshHopper") ||
        HasKeywordOrName(actor, "Wisp") ||
        HasKeywordOrName(actor, "BoneHawk")) {
        return CreatureTier::kPreyAndSmall;
    }

    return CreatureTier::kMedium;
}

ArmorWeightClass GetActorArmorClass(RE::Actor* actor) {
    if (!actor) return ArmorWeightClass::kNone;

    static const RE::BIPED_MODEL::BipedObjectSlot slots[] = {
        RE::BIPED_MODEL::BipedObjectSlot::kHead,
        RE::BIPED_MODEL::BipedObjectSlot::kBody,
        RE::BIPED_MODEL::BipedObjectSlot::kHands,
        RE::BIPED_MODEL::BipedObjectSlot::kFeet
    };

    int heavyCount = 0;
    int lightCount = 0;

    for (const auto& slot : slots) {
        auto* item = actor->GetWornArmor(slot);
        if (item) {
            if (item->IsHeavyArmor()) {
                heavyCount++;
            } else if (item->IsLightArmor()) {
                lightCount++;
            }
        }
    }

    if (heavyCount == 0 && lightCount == 0) {
        return ArmorWeightClass::kNone;
    }

    if (heavyCount >= lightCount && heavyCount > 0) {
        return ArmorWeightClass::kHeavy;
    }

    return ArmorWeightClass::kLight;
}

const StaggerProfile* ResolveActorProfile(RE::Actor* actor) {
    if (!actor) return nullptr;
    auto* settings = Settings::GetSingleton();

    bool isPlayer = actor->IsPlayerRef() || (actor == RE::PlayerCharacter::GetSingleton());

    if (isPlayer) {
        if (!settings->playerEnabled) return nullptr;

        if (IsBeastForm(actor)) {
            return &settings->creatureHeavy;
        }

        ArmorWeightClass armorClass = GetActorArmorClass(actor);
        switch (armorClass) {
            case ArmorWeightClass::kHeavy: return &settings->playerHeavy;
            case ArmorWeightClass::kLight: return &settings->playerLight;
            default: return &settings->playerNone;
        }
    } else if (IsCreature(actor)) {
        if (!settings->creatureEnabled) return nullptr;
        CreatureTier tier = GetCreatureTier(actor);
        switch (tier) {
            case CreatureTier::kColossalBoss: return &settings->creatureColossal;
            case CreatureTier::kHeavyBeast:   return &settings->creatureHeavy;
            case CreatureTier::kPreyAndSmall:  return &settings->creaturePrey;
            default:                           return &settings->creatureMedium;
        }
    } else {
        if (!settings->npcEnabled) return nullptr;
        ArmorWeightClass armorClass = GetActorArmorClass(actor);
        switch (armorClass) {
            case ArmorWeightClass::kHeavy: return &settings->npcHeavy;
            case ArmorWeightClass::kLight: return &settings->npcLight;
            default: return &settings->npcNone;
        }
    }
}

int ResolveActorDecaySeconds(RE::Actor* actor) {
    if (!actor) return 15;
    auto* settings = Settings::GetSingleton();

    bool isPlayer = actor->IsPlayerRef() || (actor == RE::PlayerCharacter::GetSingleton());

    if (isPlayer) {
        if (IsBeastForm(actor)) {
            return settings->creatureHeavyDecaySeconds;
        }
        return settings->playerComboDecaySeconds;
    } else if (IsCreature(actor)) {
        CreatureTier tier = GetCreatureTier(actor);
        switch (tier) {
            case CreatureTier::kColossalBoss: return settings->creatureColossalDecaySeconds;
            case CreatureTier::kHeavyBeast:   return settings->creatureHeavyDecaySeconds;
            case CreatureTier::kPreyAndSmall:  return settings->creaturePreyDecaySeconds;
            default:                           return settings->creatureMediumDecaySeconds;
        }
    } else {
        return settings->npcComboDecaySeconds;
    }
}

float GetActorInstantBreakThreshold(RE::Actor* actor) {
    if (!actor) return 0.0f;
    auto* settings = Settings::GetSingleton();

    bool isPlayer = actor->IsPlayerRef() || (actor == RE::PlayerCharacter::GetSingleton());

    if (isPlayer) {
        if (IsBeastForm(actor)) {
            return settings->creatureInstantBreakThreshold;
        }
        return settings->playerInstantBreakThreshold;
    } else if (IsCreature(actor)) {
        return settings->creatureInstantBreakThreshold;
    } else {
        return settings->npcInstantBreakThreshold;
    }
}

float ResolveCreatureStrikeDamage(RE::Actor* aggressor, const CreatureAttackMultipliers& mults) {
    CreatureTier tier = GetCreatureTier(aggressor);
    switch (tier) {
        case CreatureTier::kColossalBoss: return mults.colossal;
        case CreatureTier::kHeavyBeast:   return mults.heavy;
        case CreatureTier::kPreyAndSmall:  return mults.prey;
        default:                           return mults.medium;
    }
}

float ResolveWeaponMultiplier(RE::TESObjectWEAP* weap, const WeaponMultipliers& mults) {
    if (!weap) return mults.sword;

    if (weap->HasKeywordString("WeapTypeQuarterstaff") || weap->HasKeywordString("WeapTypeStaff")) {
        return mults.greatsword;
    }
    if (weap->HasKeywordString("WeapTypeHalberd")) {
        return mults.battleaxe;
    }
    if (weap->HasKeywordString("WeapTypeSpear") || weap->HasKeywordString("WeapTypePike")) {
        return mults.greatsword;
    }
    if (weap->HasKeywordString("WeapTypeRapier")) {
        return mults.sword;
    }
    if (weap->HasKeywordString("WeapTypeClaw") || weap->HasKeywordString("WeapTypeCestus")) {
        return mults.unarmed;
    }
    if (weap->HasKeywordString("WeapTypeWhip")) {
        return mults.sword;
    }

    if (weap->HasKeywordString("WeapTypeWarhammer")) {
        return mults.warhammer;
    }
    if (weap->HasKeywordString("WeapTypeBattleaxe")) {
        return mults.battleaxe;
    }
    if (weap->HasKeywordString("WeapTypeGreatsword")) {
        return mults.greatsword;
    }
    if (weap->HasKeywordString("WeapTypeMace")) {
        return mults.mace;
    }
    if (weap->HasKeywordString("WeapTypeWarAxe") || weap->HasKeywordString("WeapTypeAxe")) {
        return mults.warAxe;
    }
    if (weap->HasKeywordString("WeapTypeDagger")) {
        return mults.dagger;
    }
    if (weap->HasKeywordString("WeapTypeBow")) {
        return mults.bow;
    }
    if (weap->HasKeywordString("WeapTypeCrossbow")) {
        return mults.crossbow;
    }
    if (weap->HasKeywordString("WeapTypeSword")) {
        return mults.sword;
    }

    switch (weap->GetWeaponType()) {
        case RE::WEAPON_TYPE::kOneHandDagger: return mults.dagger;
        case RE::WEAPON_TYPE::kOneHandSword:  return mults.sword;
        case RE::WEAPON_TYPE::kOneHandAxe:    return mults.warAxe;
        case RE::WEAPON_TYPE::kOneHandMace:   return mults.mace;
        case RE::WEAPON_TYPE::kTwoHandSword:  return mults.greatsword;
        case RE::WEAPON_TYPE::kTwoHandAxe:    return mults.battleaxe;
        case RE::WEAPON_TYPE::kBow:           return mults.bow;
        case RE::WEAPON_TYPE::kCrossbow:      return mults.crossbow;
        default:                              return mults.sword;
    }
}

bool IsPassiveOrCloakSpell(RE::TESForm* a_sourceForm) {
    if (!a_sourceForm) return false;

    if (auto* spell = a_sourceForm->As<RE::SpellItem>()) {
        auto delivery = spell->data.delivery;

        if (delivery == RE::MagicSystem::Delivery::kSelf) {
            return true;
        }

        if (delivery == RE::MagicSystem::Delivery::kTargetLocation) {
            return true;
        }

        if (spell->data.spellType == RE::MagicSystem::SpellType::kAbility ||
            spell->data.castingType == RE::MagicSystem::CastingType::kConstantEffect) {
            return true;
        }
    }

    return false;
}

bool IsWardDeflectingSpell(RE::Actor* victim) {
    if (!victim) return false;

    auto* magicTarget = victim->AsMagicTarget();
    if (!magicTarget) return false;

    auto* activeEffects = magicTarget->GetActiveEffectList();
    if (!activeEffects) return false;

    for (auto* effect : *activeEffects) {
        if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive)) continue;
        auto* baseEffect = effect->GetBaseObject();
        if (!baseEffect) continue;

        if (baseEffect->HasKeywordString("MagicWard") ||
            baseEffect->data.primaryAV == RE::ActorValue::kWardDeflection ||
            baseEffect->data.primaryAV == RE::ActorValue::kWardPower ||
            baseEffect->data.archetype == RE::EffectArchetypes::ArchetypeID::kAccumulateMagnitude) {
            return true;
        }
    }

    return false;
}

bool IsEthereal(RE::Actor* actor) {
    if (!actor) return false;

    auto* magicTarget = actor->AsMagicTarget();
    if (magicTarget) {
        auto* activeEffects = magicTarget->GetActiveEffectList();
        if (activeEffects) {
            for (auto* effect : *activeEffects) {
                if (!effect || 
                    effect->flags.any(RE::ActiveEffect::Flag::kInactive) || 
                    effect->flags.any(RE::ActiveEffect::Flag::kDispelled)) {
                    continue;
                }

                auto* baseEffect = effect->GetBaseObject();
                if (!baseEffect) continue;

                if (baseEffect->data.archetype == RE::EffectArchetypes::ArchetypeID::kEtherealize) {
                    auto* spell = effect->spell ? effect->spell->As<RE::SpellItem>() : nullptr;
                    if (spell && spell->data.spellType != RE::MagicSystem::SpellType::kAbility) {
                        return true;
                    }
                }

                if (baseEffect->HasKeywordString("Etherealize") || 
                    baseEffect->HasKeywordString("BecomeEtherealKeyword")) {
                    return true;
                }
            }
        }
    }

    return false;
}

float GetEffectivePhysicalDamage(RE::Actor* aggressor, RE::TESForm* sourceForm, const Settings* settings) {
    if (!aggressor) return 10.0f;

    auto* avOwner = aggressor->AsActorValueOwner();
    float rawDmg = 10.0f;

    auto applySkillScaling = [&](RE::TESObjectWEAP* weap) {
        if (!weap) return;
        rawDmg = static_cast<float>(weap->GetAttackDamage());

        if (avOwner) {
            auto weapType = weap->GetWeaponType();
            switch (weapType) {
                case RE::WEAPON_TYPE::kOneHandDagger:
                case RE::WEAPON_TYPE::kOneHandSword:
                case RE::WEAPON_TYPE::kOneHandAxe:
                case RE::WEAPON_TYPE::kOneHandMace: {
                    float skill = avOwner->GetActorValue(RE::ActorValue::kOneHanded);
                    rawDmg *= (1.0f + (skill * 0.005f));
                    break;
                }
                case RE::WEAPON_TYPE::kTwoHandSword:
                case RE::WEAPON_TYPE::kTwoHandAxe: {
                    float skill = avOwner->GetActorValue(RE::ActorValue::kTwoHanded);
                    rawDmg *= (1.0f + (skill * 0.005f));
                    break;
                }
                case RE::WEAPON_TYPE::kBow:
                case RE::WEAPON_TYPE::kCrossbow: {
                    float skill = avOwner->GetActorValue(RE::ActorValue::kArchery);
                    rawDmg *= (1.0f + (skill * 0.005f));
                    break;
                }
                default:
                    break;
            }
        }
    };

    if (sourceForm && sourceForm->Is(RE::FormType::Weapon)) {
        auto* weap = sourceForm->As<RE::TESObjectWEAP>();
        applySkillScaling(weap);
    } else {
        auto* attackingWeap = aggressor->GetAttackingWeapon();
        if (attackingWeap && attackingWeap->object && attackingWeap->object->Is(RE::FormType::Weapon)) {
            auto* weap = attackingWeap->object->As<RE::TESObjectWEAP>();
            applySkillScaling(weap);
        } else {
            auto* rightObj = aggressor->GetEquippedObject(false);
            auto* leftObj = aggressor->GetEquippedObject(true);
            if (rightObj && rightObj->Is(RE::FormType::Weapon)) {
                rawDmg = static_cast<float>(rightObj->As<RE::TESObjectWEAP>()->GetAttackDamage());
            } else if (leftObj && leftObj->Is(RE::FormType::Weapon)) {
                rawDmg = static_cast<float>(leftObj->As<RE::TESObjectWEAP>()->GetAttackDamage());
            } else if (IsCreature(aggressor) || IsBeastForm(aggressor)) {
                rawDmg = 20.0f * ResolveCreatureStrikeDamage(aggressor, settings->creatureAttacks);
            }
        }
    }

    return (rawDmg > 1.0f ? rawDmg : 1.0f);
}

float GetHitBufferDamage(RE::Actor* aggressor, const RE::TESHitEvent* a_event) {
    if (!aggressor) return 1.0f;
    auto* settings = Settings::GetSingleton();
    const auto& mults = settings->multipliers;

    RE::TESForm* sourceForm = a_event ? RE::TESForm::LookupByID(a_event->source) : nullptr;

    if (sourceForm && sourceForm->Is(RE::FormType::Weapon)) {
        float damage = ResolveWeaponMultiplier(sourceForm->As<RE::TESObjectWEAP>(), mults);
        if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) damage *= mults.powerAttackMult;
        if (a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack)) damage *= mults.sneakAttackMult;
        return damage;
    }

    if (sourceForm && sourceForm->Is(RE::FormType::Spell)) {
        return mults.magic;
    }

    if (sourceForm && sourceForm->Is(RE::FormType::Ammo)) {
        float damage = mults.bow;
        auto* equipped = aggressor->GetAttackingWeapon();
        if (equipped && equipped->object && equipped->object->Is(RE::FormType::Weapon)) {
            damage = ResolveWeaponMultiplier(equipped->object->As<RE::TESObjectWEAP>(), mults);
        } else {
            auto* rightObj = aggressor->GetEquippedObject(false);
            if (rightObj && rightObj->Is(RE::FormType::Weapon)) {
                damage = ResolveWeaponMultiplier(rightObj->As<RE::TESObjectWEAP>(), mults);
            }
        }
        if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) damage *= mults.powerAttackMult;
        if (a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack)) damage *= mults.sneakAttackMult;
        return damage;
    }

    auto* state = aggressor->AsActorState();
    bool isBash = a_event->flags.any(RE::TESHitEvent::Flag::kBashAttack) || 
                  (state && state->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash);

    if (isBash) {
        if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) {
            return mults.powerBashMult;
        }
        return mults.bashMult;
    }

    float damage = 1.0f;
    auto* equipped = aggressor->GetAttackingWeapon();
    if (equipped && equipped->object && equipped->object->Is(RE::FormType::Weapon)) {
        damage = ResolveWeaponMultiplier(equipped->object->As<RE::TESObjectWEAP>(), mults);
    } else {
        auto* rightObj = aggressor->GetEquippedObject(false);
        auto* leftObj = aggressor->GetEquippedObject(true);

        if (rightObj && rightObj->Is(RE::FormType::Weapon)) {
            damage = ResolveWeaponMultiplier(rightObj->As<RE::TESObjectWEAP>(), mults);
        } else if (leftObj && leftObj->Is(RE::FormType::Weapon)) {
            damage = ResolveWeaponMultiplier(leftObj->As<RE::TESObjectWEAP>(), mults);
        } else if (IsCreature(aggressor) || IsBeastForm(aggressor)) {
            damage = ResolveCreatureStrikeDamage(aggressor, settings->creatureAttacks);
        } else {
            damage = mults.unarmed;
        }
    }

    if (a_event) {
        if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) damage *= mults.powerAttackMult;
        if (a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack)) damage *= mults.sneakAttackMult;
    }

    return damage;
}

class ComboStaggerTracker {
public:
    static ComboStaggerTracker* GetSingleton() {
        static ComboStaggerTracker instance;
        return &instance;
    }

    struct StaggerState {
        float bufferHits = 0.0f;
        int staggerCount = 0;
        std::chrono::steady_clock::time_point lastHitTime{};
        std::chrono::steady_clock::time_point cooldownStart{};
        std::chrono::steady_clock::time_point cooldownEnd{};
        std::chrono::steady_clock::time_point lastFlashTime{};
        bool phantomOverridden = false;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> lastSpellHitTimes{};
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> lastAggressorHitTimes{};

        void UpdateDecay(const std::chrono::steady_clock::time_point& now, int decaySeconds) {
            if (now < cooldownEnd) return;

            if (cooldownEnd != std::chrono::steady_clock::time_point{} && lastHitTime < cooldownEnd) {
                lastHitTime = std::chrono::steady_clock::time_point{};
                bufferHits = 0.0f;
                staggerCount = 0;
                return;
            }

            if (decaySeconds <= 0 || lastHitTime == std::chrono::steady_clock::time_point{} || bufferHits <= 0.0f) {
                return;
            }

            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHitTime).count();
            auto decayTotalMs = static_cast<int64_t>(decaySeconds * 1000);

            if (elapsedMs >= decayTotalMs) {
                bufferHits = 0.0f;
                staggerCount = 0;
                lastHitTime = std::chrono::steady_clock::time_point{};
            }
        }
    };

    void ResetAll() {
        std::unique_lock lock(mutex);
        trackerMap.clear();
    }

    void PruneStale() {
        auto now = std::chrono::steady_clock::now();
        std::unique_lock lock(mutex);
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPruneTime).count() < 30) {
            return;
        }
        lastPruneTime = now;

        for (auto it = trackerMap.begin(); it != trackerMap.end();) {
            if (it->first == 0x14) {
                ++it;
                continue;
            }

            const auto& state = it->second;
            bool onCd = (now < state.cooldownEnd);
            auto idleSec = std::chrono::duration_cast<std::chrono::seconds>(now - state.lastHitTime).count();

            if (!onCd && idleSec > 60) {
                it = trackerMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool IsAggressorOnSwingCooldown(RE::FormID victimID, RE::FormID aggressorID) {
        std::unique_lock lock(mutex);
        auto& state = trackerMap[victimID];
        auto now = std::chrono::steady_clock::now();

        auto it = state.lastAggressorHitTimes.find(aggressorID);
        if (it != state.lastAggressorHitTimes.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            if (elapsed < 15) {
                return true;
            }
        }

        state.lastAggressorHitTimes[aggressorID] = now;
        return false;
    }

    bool IsSpellOnCooldown(RE::FormID victimID, RE::FormID spellID, int cooldownMs) {
        if (cooldownMs <= 0) return false;

        std::unique_lock lock(mutex);
        auto& state = trackerMap[victimID];
        auto now = std::chrono::steady_clock::now();

        auto it = state.lastSpellHitTimes.find(spellID);
        if (it != state.lastSpellHitTimes.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            if (elapsed < cooldownMs) {
                return true;
            }
        }

        state.lastSpellHitTimes[spellID] = now;
        return false;
    }

    float GetDescendingBufferDisplay(RE::Actor* actor, int maxBuffer, int decaySeconds) {
        if (!actor) return static_cast<float>(maxBuffer);
        RE::FormID formID = GetCanonicalFormID(actor);

        std::unique_lock lock(mutex);
        auto it = trackerMap.find(formID);
        if (it == trackerMap.end()) {
            return static_cast<float>(maxBuffer);
        }

        auto now = std::chrono::steady_clock::now();
        float maxF = static_cast<float>(maxBuffer);

        if (now < it->second.cooldownEnd) {
            auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(it->second.cooldownEnd - it->second.cooldownStart).count();
            if (totalMs > 0) {
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.cooldownStart).count();
                float progress = (std::clamp)(static_cast<float>(elapsedMs) / static_cast<float>(totalMs), 0.0f, 1.0f);
                return progress * maxF;
            }
            return 0.0f;
        }

        it->second.UpdateDecay(now, decaySeconds);

        float staticRemaining = (std::clamp)(maxF - it->second.bufferHits, 0.0f, maxF);

        float visualDisplayValue = staticRemaining;
        if (decaySeconds > 0 && it->second.lastHitTime != std::chrono::steady_clock::time_point{} && it->second.bufferHits > 0.0f) {
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastHitTime).count();
            auto decayTotalMs = static_cast<int64_t>(decaySeconds * 1000);

            if (elapsedMs < decayTotalMs) {
                float visualProgress = (std::clamp)(static_cast<float>(elapsedMs) / static_cast<float>(decayTotalMs), 0.0f, 1.0f);
                visualDisplayValue = staticRemaining + (visualProgress * (maxF - staticRemaining));
            } else {
                visualDisplayValue = maxF;
            }
        }

        bool isStaggerVulnerable = (it->second.bufferHits >= maxF || it->second.staggerCount > 0);

        if (visualDisplayValue >= maxF) {
            if (it->second.phantomOverridden && g_trueHUD) {
                g_trueHUD->RevertSpecialBarColor(actor->GetHandle(), TRUEHUD_API::BarColorType::PhantomColor);
                it->second.phantomOverridden = false;
            }
        }

        if (isStaggerVulnerable && visualDisplayValue < maxF) {
            if (g_trueHUD && g_pluginHandle != SKSE::kInvalidPluginHandle) {
                auto flashElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastFlashTime).count();
                if (flashElapsedMs >= 350) {
                    it->second.lastFlashTime = now;
                    g_trueHUD->FlashActorSpecialBar(g_pluginHandle, actor->GetHandle(), false);
                }
            }
        }

        return visualDisplayValue;
    }

    bool ProcessHit(RE::Actor* victim, const StaggerProfile& profile, int decaySeconds, float bufferDamage) {
        if (!victim || bufferDamage <= 0.0f) return false;
        RE::FormID formID = GetCanonicalFormID(victim);

        std::unique_lock lock(mutex);
        auto& state = trackerMap[formID];
        auto now = std::chrono::steady_clock::now();

        if (now < state.cooldownEnd) {
            return false;
        }

        state.UpdateDecay(now, decaySeconds);

        state.lastHitTime = now;
        state.phantomOverridden = false;

        float maxBuf = static_cast<float>(profile.hitBuffer);
        if (state.bufferHits < maxBuf) {
            state.bufferHits += bufferDamage;
            if (state.bufferHits < maxBuf) {
                return false;
            }
        }

        state.staggerCount++;

        if (state.staggerCount >= profile.maxStaggers) {
            state.bufferHits = 0.0f;
            state.staggerCount = 0;
            state.cooldownStart = now;
            state.cooldownEnd = now + std::chrono::seconds(profile.cooldownSeconds);
            state.lastHitTime = std::chrono::steady_clock::time_point{};
        }

        return true;
    }

private:
    std::shared_mutex mutex;
    std::chrono::steady_clock::time_point lastPruneTime{};
    std::unordered_map<RE::FormID, StaggerState> trackerMap;
};

class TrueHUDManager {
public:
    static TrueHUDManager* GetSingleton() {
        static TrueHUDManager instance;
        return &instance;
    }

    void Initialize() {
        if (_initialized) return;

        if (!g_trueHUD) {
            g_trueHUD = reinterpret_cast<TRUEHUD_API::IVTrueHUD3*>(
                TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V3));
        }

        if (g_trueHUD && g_pluginHandle != SKSE::kInvalidPluginHandle) {
            auto result = g_trueHUD->RequestSpecialResourceBarsControl(g_pluginHandle);

            if (result == TRUEHUD_API::APIResult::OK || result == TRUEHUD_API::APIResult::AlreadyGiven) {
                auto getCurrent = [](RE::Actor* a_actor) -> float {
                    if (!a_actor || a_actor->IsDead()) return 0.0f;
                    auto* settings = Settings::GetSingleton();
                    if (!settings->enabled || !settings->enableTrueHUD) return 0.0f;

                    const auto* profile = ResolveActorProfile(a_actor);
                    if (!profile || profile->hitBuffer <= 0) return 0.0f;

                    int decay = ResolveActorDecaySeconds(a_actor);
                    return ComboStaggerTracker::GetSingleton()->GetDescendingBufferDisplay(
                        a_actor, profile->hitBuffer, decay);
                };

                auto getMax = [](RE::Actor* a_actor) -> float {
                    if (!a_actor || a_actor->IsDead()) return 0.0f;
                    auto* settings = Settings::GetSingleton();
                    if (!settings->enabled || !settings->enableTrueHUD) return 0.0f;

                    const auto* profile = ResolveActorProfile(a_actor);
                    if (!profile || profile->hitBuffer <= 0) return 0.0f;

                    return static_cast<float>(profile->hitBuffer);
                };

                g_trueHUD->RegisterSpecialResourceFunctions(g_pluginHandle, getCurrent, getMax, true, true);
                _initialized = true;
                SKSE::log::info("Combo Stagger: TrueHUD Special Resource Bar successfully hooked!");
            }
        }
    }

    void FlashBar(RE::ActorHandle a_handle) {
        if (g_trueHUD && a_handle && g_pluginHandle != SKSE::kInvalidPluginHandle) {
            g_trueHUD->FlashActorSpecialBar(g_pluginHandle, a_handle, false);
        }
    }

private:
    bool _initialized = false;
};

void ApplyStagger(RE::Actor* victim, RE::Actor* aggressor, float magnitude) {
    if (!victim || victim->IsDead()) return;
    if (victim->IsOnMount() || victim->IsBeingRidden()) return;

    auto* state = victim->AsActorState();
    if (state) {
        if (state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
            state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
            return;
        }
    }

    float staggerDir = 0.0f;

    if (aggressor) {
        auto victimPos = victim->GetPosition();
        auto aggPos = aggressor->GetPosition();

        float dx = aggPos.x - victimPos.x;
        float dy = aggPos.y - victimPos.y;

        float angleToAggressor = std::atan2(dx, dy);
        float relativeAngle = angleToAggressor - victim->GetAngleZ();

        constexpr float twoPi = 6.28318530718f;
        while (relativeAngle < 0.0f) relativeAngle += twoPi;
        while (relativeAngle >= twoPi) relativeAngle -= twoPi;

        staggerDir = relativeAngle / twoPi;
        victim->SetGraphVariableFloat("staggerHeading", relativeAngle);
    }

    victim->SetGraphVariableFloat("staggerDirection", staggerDir);
    victim->SetGraphVariableFloat("staggerMagnitude", magnitude);
    victim->NotifyAnimationGraph("staggerStart");
}

class HitEventHandler : public RE::BSTEventSink<RE::TESHitEvent> {
public:
    static HitEventHandler* GetSingleton() {
        static HitEventHandler instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override {
        auto* settings = Settings::GetSingleton();

        if (!settings->enabled || !a_event || !a_event->target || !a_event->cause) {
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::TESForm* sourceForm = RE::TESForm::LookupByID(a_event->source);

        if (sourceForm) {
            auto formType = sourceForm->GetFormType();
            if (formType == RE::FormType::MagicEffect || 
                formType == RE::FormType::Enchantment || 
                formType == RE::FormType::Scroll) {
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        if (IsPassiveOrCloakSpell(sourceForm)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* victim = a_event->target->As<RE::Actor>();
        auto* aggressor = a_event->cause->As<RE::Actor>();

        if (!victim || !aggressor || victim->IsDead()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (IsEthereal(victim) || IsEthereal(aggressor)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (victim->IsInKillMove() || aggressor->IsInKillMove()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (victim->IsOnMount() || victim->IsBeingRidden()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (sourceForm && sourceForm->Is(RE::FormType::Spell)) {
            if (IsWardDeflectingSpell(victim)) {
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        const auto* profile = ResolveActorProfile(victim);
        if (!profile) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* tracker = ComboStaggerTracker::GetSingleton();
        tracker->PruneStale();

        RE::FormID canonVictimID = GetCanonicalFormID(victim);
        RE::FormID canonAggressorID = GetCanonicalFormID(aggressor);

        if (!sourceForm || !sourceForm->Is(RE::FormType::Spell)) {
            if (tracker->IsAggressorOnSwingCooldown(canonVictimID, canonAggressorID)) {
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        if (sourceForm && sourceForm->Is(RE::FormType::Spell)) {
            if (tracker->IsSpellOnCooldown(canonVictimID, sourceForm->GetFormID(), settings->multipliers.spellHitCooldownMs)) {
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        float bufferDamage = GetHitBufferDamage(aggressor, a_event);

        bool isBlocked = a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked);
        if (isBlocked) {
            float blockFactor = (std::clamp)(settings->blockMitigationPercent, 0.0f, 1.0f);
            bufferDamage *= (1.0f - blockFactor);

            if (bufferDamage > 0.0f) {
                int decaySeconds = ResolveActorDecaySeconds(victim);
                tracker->ProcessHit(victim, *profile, decaySeconds, bufferDamage);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        float threshold = GetActorInstantBreakThreshold(victim);
        if (threshold > 0.0f) {
            auto* avOwner = victim->AsActorValueOwner();
            float maxHP = avOwner ? avOwner->GetPermanentActorValue(RE::ActorValue::kHealth) : 100.0f;
            if (maxHP > 0.0f) {
                float rawDmg = GetEffectivePhysicalDamage(aggressor, sourceForm, settings);

                if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) {
                    rawDmg *= settings->multipliers.powerAttackMult;
                }
                if (a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack)) {
                    rawDmg *= settings->multipliers.sneakAttackMult;
                }

                float armorRating = avOwner ? avOwner->GetActorValue(RE::ActorValue::kDamageResist) : 0.0f;
                float damageReduction = (std::clamp)(armorRating * 0.0012f, 0.0f, 0.80f);
                float finalDamage = rawDmg * (1.0f - damageReduction);

                if ((finalDamage / maxHP) >= threshold) {
                    bufferDamage = static_cast<float>(profile->hitBuffer);
                }
            }
        }

        int decaySeconds = ResolveActorDecaySeconds(victim);
        if (tracker->ProcessHit(victim, *profile, decaySeconds, bufferDamage)) {
            ApplyStagger(victim, aggressor, profile->staggerMagnitude);
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

void RenderProfileSliders(const char* labelPrefix, StaggerProfile& profile, Settings* settings) {
    std::string bufLabel = std::string("Hit Buffer##") + labelPrefix;
    std::string stagLabel = std::string("Max Staggers##") + labelPrefix;
    std::string cdLabel = std::string("Cooldown (Sec)##") + labelPrefix;
    std::string magLabel = std::string("Stagger Magnitude##") + labelPrefix;

    if (ImGui::SliderInt(bufLabel.c_str(), &profile.hitBuffer, 0, 60)) {
        settings->Save();
    }
    if (ImGui::SliderInt(stagLabel.c_str(), &profile.maxStaggers, 1, 10)) {
        settings->Save();
    }
    if (ImGui::SliderInt(cdLabel.c_str(), &profile.cooldownSeconds, 1, 60)) {
        settings->Save();
    }
    if (ImGui::SliderFloat(magLabel.c_str(), &profile.staggerMagnitude, 0.1f, 8.0f, "%.2f")) {
        settings->Save();
    }
}

void __stdcall RenderGeneralMenu() {
    auto* settings = Settings::GetSingleton();

    ImGui::Text("Combo Stagger - Master Controls");
    ImGui::Separator();

    if (ImGui::Checkbox("Master Enable", &settings->enabled)) {
        settings->Save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("HUD & Integrations");

    if (ImGui::Checkbox("Enable TrueHUD Special Bar", &settings->enableTrueHUD)) {
        settings->Save();
        if (settings->enableTrueHUD) {
            TrueHUDManager::GetSingleton()->Initialize();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Hooks the TrueHUD Special Bar to visually display descending Hit Buffer poise.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Global Combat Rules");

    if (ImGui::SliderFloat("Block Poise Mitigation (% Absorbed)", &settings->blockMitigationPercent, 0.0f, 1.0f, "%.2f")) {
        settings->Save();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Portion of incoming Hit Buffer damage absorbed by successful blocks.\n1.00 = 100% absorbed (complete poise immunity while blocking).\n0.00 = 0% absorbed (blocks take full poise damage).\nSuccessful blocks also prevent Instant Break thresholds from triggering.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("INI Configuration");

    if (ImGui::Button("Reset to Default Values")) {
        settings->ResetToDefaults();
        settings->Save();
        RE::DebugNotification("Combo Stagger: Reset to original defaults and saved to INI!");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Restores hardcoded default values and overwrites the active INI file.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Utilities");

    if (ImGui::Button("Reset All Active Cooldowns & Trackers")) {
        ComboStaggerTracker::GetSingleton()->ResetAll();
        RE::DebugNotification("Combo Stagger: All active cooldowns & buffers cleared!");
    }
}

void __stdcall RenderComboDecayMenu() {
    auto* settings = Settings::GetSingleton();

    ImGui::Text("Combo Decay & Idle Regeneration Timers");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Controls the idle time required before Hit Buffer stacks begin regenerating back to full.");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Humanoid Actors", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("Player Decay Timer (Seconds)", &settings->playerComboDecaySeconds, 1, 60)) {
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Idle time required without taking damage before the Player's Hit Buffer begins regenerating.");
        }

        if (ImGui::SliderInt("NPC Decay Timer (Seconds)", &settings->npcComboDecaySeconds, 1, 60)) {
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Idle time required without taking damage before humanoid NPCs' Hit Buffer begins regenerating.");
        }
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Creatures & Monsters by Tier", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("Colossal & Bosses Decay (Sec)", &settings->creatureColossalDecaySeconds, 1, 60)) {
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Dragons, Giants, Dwarven Centurions, Mammoths, Gargoyles, Lurkers, Karstaag, Dwarven Ballistas, Ash Guardians, Soul Cairn Keepers/Reapers, Seekers");
        }

        if (ImGui::SliderInt("Heavy Beasts Decay (Sec)", &settings->creatureHeavyDecaySeconds, 1, 60)) {
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Trolls, Bears, Sabrecats, Werewolves, Werebears, Vampire Lords, Dwarven Spheres, Dragon Priests, Spriggans, Chaurus, Bristlebacks, Draugr Deathlords, Overlords, Hulking Draugr");
        }

        if (ImGui::SliderInt("Medium Monsters Decay (Sec)", &settings->creatureMediumDecaySeconds, 1, 60)) {
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Death Hounds, Ash Spawn, Atronachs, Hagravens, Standard Draugr, Skeletons, Wolves, Horkers, Falmer, Frostbite Spiders");
        }

        if (ImGui::SliderInt("Prey & Small Animals Decay (Sec)", &settings->creaturePreyDecaySeconds, 1, 60)) {
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Slaughterfish, Rieklings, Ash Hoppers, Wisps, Hares/Rabbits, Goats, Chickens, Spider Centurions, Bone Hawks, Deer, Elk, Foxes, Skeevers, Mudcrabs, Dwarven Spiders");
        }
    }
}

void __stdcall RenderPlayerMenu() {
    auto* settings = Settings::GetSingleton();

    ImGui::Text("Player Stagger Configuration");
    ImGui::Separator();

    if (ImGui::Checkbox("Enable for Player", &settings->playerEnabled)) {
        settings->Save();
    }

    if (!settings->playerEnabled) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Player staggering is currently disabled.");
        return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Damage Rules");

    if (ImGui::SliderFloat("Player Instant Break Threshold (% Max HP Damage)", &settings->playerInstantBreakThreshold, 0.0f, 1.0f, "%.2f")) {
        settings->Save();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("If post-mitigation hit damage deals this percentage or more of Max HP (e.g. 0.50 = 50%), all Hit Buffer stacks are instantly depleted. Set to 0.00 to disable.");
    }

    ImGui::Spacing();
    ImGui::Separator();

    bool openHeavy = ImGui::CollapsingHeader("Heavy Armor", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openHeavy) {
        RenderProfileSliders("PlayerHeavy", settings->playerHeavy, settings);
    }

    ImGui::Spacing();

    bool openLight = ImGui::CollapsingHeader("Light Armor", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openLight) {
        RenderProfileSliders("PlayerLight", settings->playerLight, settings);
    }

    ImGui::Spacing();

    bool openUnarmored = ImGui::CollapsingHeader("Unarmored / Clothing", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openUnarmored) {
        RenderProfileSliders("PlayerUnarmored", settings->playerNone, settings);
    }
}

void __stdcall RenderNPCMenu() {
    auto* settings = Settings::GetSingleton();

    ImGui::Text("Humanoid NPC Stagger Configuration");
    ImGui::Separator();

    if (ImGui::Checkbox("Enable for Humanoid NPCs", &settings->npcEnabled)) {
        settings->Save();
    }

    if (!settings->npcEnabled) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "NPC staggering is currently disabled.");
        return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Damage Rules");

    if (ImGui::SliderFloat("NPC Instant Break Threshold (% Max HP Damage)", &settings->npcInstantBreakThreshold, 0.0f, 1.0f, "%.2f")) {
        settings->Save();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("If post-mitigation hit damage deals this percentage or more of Max HP (e.g. 0.50 = 50%), all Hit Buffer stacks are instantly depleted. Set to 0.00 to disable.");
    }

    ImGui::Spacing();
    ImGui::Separator();

    bool openHeavy = ImGui::CollapsingHeader("Heavy Armor", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openHeavy) {
        RenderProfileSliders("NPCHeavy", settings->npcHeavy, settings);
    }

    ImGui::Spacing();

    bool openLight = ImGui::CollapsingHeader("Light Armor", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openLight) {
        RenderProfileSliders("NPCLight", settings->npcLight, settings);
    }

    ImGui::Spacing();

    bool openUnarmored = ImGui::CollapsingHeader("Unarmored / Clothing", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openUnarmored) {
        RenderProfileSliders("NPCUnarmored", settings->npcNone, settings);
    }
}

void __stdcall RenderCreatureMenu() {
    auto* settings = Settings::GetSingleton();
    auto& cAttacks = settings->creatureAttacks;

    ImGui::Text("Creature & Monster Stagger Configuration");
    ImGui::Separator();

    if (ImGui::Checkbox("Enable for Creatures", &settings->creatureEnabled)) {
        settings->Save();
    }

    if (!settings->creatureEnabled) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Creature staggering is currently disabled.");
        return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Damage Rules");

    if (ImGui::SliderFloat("Creature Instant Break Threshold (% Max HP Damage)", &settings->creatureInstantBreakThreshold, 0.0f, 1.0f, "%.2f")) {
        settings->Save();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("If post-mitigation hit damage deals this percentage or more of Max HP (e.g. 0.50 = 50%), all Hit Buffer stacks are instantly depleted. Set to 0.00 to disable.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Creature Outgoing Attack Poise Damage");

    if (ImGui::CollapsingHeader("Outgoing Strike Multipliers", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderFloat("Colossal & Bosses Outgoing", &cAttacks.colossal, 0.1f, 20.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Heavy Beasts Outgoing", &cAttacks.heavy, 0.1f, 20.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Medium Monsters Outgoing", &cAttacks.medium, 0.1f, 20.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Prey & Small Animals Outgoing", &cAttacks.prey, 0.1f, 20.0f, "%.2f")) settings->Save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Creature Defense & Stagger Profiles");

    bool openColossal = ImGui::CollapsingHeader("Colossal & Bosses", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Dragons, Giants, Dwarven Centurions, Mammoths, Gargoyles, Lurkers, Karstaag, Dwarven Ballistas, Ash Guardians, Soul Cairn Keepers/Reapers, Seekers");
    }
    if (openColossal) {
        RenderProfileSliders("CreatureColossal", settings->creatureColossal, settings);
    }

    ImGui::Spacing();

    bool openHeavy = ImGui::CollapsingHeader("Heavy Beasts", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Trolls, Bears, Sabrecats, Werewolves, Werebears, Vampire Lords, Dwarven Spheres, Dragon Priests, Spriggans, Chaurus, Bristlebacks, Draugr Deathlords, Overlords, Hulking Draugr");
    }
    if (openHeavy) {
        RenderProfileSliders("CreatureHeavy", settings->creatureHeavy, settings);
    }

    ImGui::Spacing();

    bool openMedium = ImGui::CollapsingHeader("Medium Monsters & Beasts", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Death Hounds, Ash Spawn, Atronachs, Hagravens, Standard Draugr, Skeletons, Wolves, Horkers, Falmer, Frostbite Spiders");
    }
    if (openMedium) {
        RenderProfileSliders("CreatureMedium", settings->creatureMedium, settings);
    }

    ImGui::Spacing();

    bool openPrey = ImGui::CollapsingHeader("Prey & Small Animals", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Slaughterfish, Rieklings, Ash Hoppers, Wisps, Hares/Rabbits, Goats, Chickens, Spider Centurions, Bone Hawks, Deer, Elk, Foxes, Skeevers, Mudcrabs, Dwarven Spiders");
    }
    if (openPrey) {
        RenderProfileSliders("CreaturePrey", settings->creaturePrey, settings);
    }
}

void __stdcall RenderWeaponMultipliersMenu() {
    auto* settings = Settings::GetSingleton();
    auto& m = settings->multipliers;

    ImGui::Text("Weapon & Damage Type Multipliers");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Controls how much Hit Buffer poise is removed per strike.");
    ImGui::Separator();

    bool openMelee = ImGui::CollapsingHeader("Melee Weapons & Unarmed", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openMelee) {
        if (ImGui::SliderFloat("Daggers", &m.dagger, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Swords", &m.sword, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("War Axes", &m.warAxe, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Maces", &m.mace, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Greatswords", &m.greatsword, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Battleaxes", &m.battleaxe, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Warhammers", &m.warhammer, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Humanoid Unarmed / Fists", &m.unarmed, 0.1f, 10.0f, "%.2f")) settings->Save();
    }

    ImGui::Spacing();

    bool openRanged = ImGui::CollapsingHeader("Ranged & Magic", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openRanged) {
        if (ImGui::SliderFloat("Bows", &m.bow, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Crossbows", &m.crossbow, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Magic / Spells", &m.magic, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderInt("Spell Hit Cooldown (ms)", &m.spellHitCooldownMs, 100, 2000)) settings->Save();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimum time (in milliseconds) required between magic poise damage ticks from continuous or multi-hit spells.");
        }
    }

    ImGui::Spacing();

    bool openBash = ImGui::CollapsingHeader("Bashing & Shield Strikes", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openBash) {
        if (ImGui::SliderFloat("Standard Bash", &m.bashMult, 0.1f, 10.0f, "%.2f")) settings->Save();
        if (ImGui::SliderFloat("Power Bash", &m.powerBashMult, 0.1f, 10.0f, "%.2f")) settings->Save();
    }

    ImGui::Spacing();

    bool openAttackMods = ImGui::CollapsingHeader("Attack Modifiers", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
    if (openAttackMods) {
        if (ImGui::SliderFloat("Power Attack Multiplier", &m.powerAttackMult, 1.0f, 10.0f, "%.2fx")) settings->Save();
        if (ImGui::SliderFloat("Sneak Attack Multiplier", &m.sneakAttackMult, 1.0f, 10.0f, "%.2fx")) settings->Save();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    g_pluginHandle = a_skse->GetPluginHandle();
    Settings::GetSingleton()->Load();

    auto* messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
            if (a_msg->type == SKSE::MessagingInterface::kPostLoad || 
                a_msg->type == SKSE::MessagingInterface::kPostPostLoad) {
                TrueHUDManager::GetSingleton()->Initialize();
            } else if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
                TrueHUDManager::GetSingleton()->Initialize();

                auto* hitSource = RE::ScriptEventSourceHolder::GetSingleton();
                if (hitSource) {
                    hitSource->AddEventSink(HitEventHandler::GetSingleton());
                }

                SKSEMenuFramework::SetSection("Combo Stagger");
                SKSEMenuFramework::AddSectionItem("General Settings", RenderGeneralMenu);
                SKSEMenuFramework::AddSectionItem("Combo Decay Settings", RenderComboDecayMenu);
                SKSEMenuFramework::AddSectionItem("Player Settings", RenderPlayerMenu);
                SKSEMenuFramework::AddSectionItem("Humanoid NPC Settings", RenderNPCMenu);
                SKSEMenuFramework::AddSectionItem("Creature Settings", RenderCreatureMenu);
                SKSEMenuFramework::AddSectionItem("Weapon & Damage Multipliers", RenderWeaponMultipliersMenu);
            } else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame ||
                       a_msg->type == SKSE::MessagingInterface::kNewGame) {
                ComboStaggerTracker::GetSingleton()->ResetAll();
            }
        });
    }

    return true;
}