#include "ExtraCommandsModule.h"
#include "ExtraCommandsModuleConfig.h"

// CMaNGOS core includes
#include "Globals/ObjectMgr.h"
#include "Guilds/GuildMgr.h"
#include "Guilds/Guild.h"
#include "Entities/Player.h"
#include "World/World.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"
#include "Database/DatabaseEnv.h"

#include <sstream>
#include <map>

namespace cmangos_module
{

// =============================================================================
// Guild flavor profiles — must match sync_guild_strategies.sh
// =============================================================================

struct FlavorProfile { std::string co, nc, react; };

static const std::map<std::string, FlavorProfile> s_flavorProfiles =
{
    { "leveling", { "+dps,+dps assist,-threat,+custom::say",
                    "+rpg,+quest,+grind,+loot,+wander,+custom::say",
                    "" } },
    { "quest",    { "+dps,+dps assist,-threat,+custom::say",
                    "+rpg,+rpg quest,+loot,+tfish,+wander,+custom::say",
                    "" } },
    { "pvp",      { "+dps,+dps assist,+threat,+boost,+pvp,+duel,+custom::say",
                    "+rpg,+wander,+bg,+custom::say",
                    "+pvp" } },
    { "farming",  { "+dps,-threat",
                    "+gather,+grind,+loot,+tfish,+wander,+rpg maintenance",
                    "" } },
};

// =============================================================================
// Module registration and lifecycle
// =============================================================================

ExtraCommandsModule::ExtraCommandsModule()
    : Module("ExtraCommandsModule", new ExtraCommandsModuleConfig())
{
}

void ExtraCommandsModule::OnInitialize()
{
    const ExtraCommandsModuleConfig* cfg =
        static_cast<const ExtraCommandsModuleConfig*>(GetConfig());

    if (!cfg->enabled)
    {
        sLog.outString("[ExtraCommands] Module disabled in config.");
        return;
    }

    sLog.outString("[ExtraCommands] Module initialized. Command prefix: .ec");
    sLog.outString("[ExtraCommands] Guild: guildmotd, guildinfo, guildpnote, guildoffnote, guildlist, guildcount, guildempty, guildflavor");
#ifdef HAVE_PLAYERBOTS
    sLog.outString("[ExtraCommands] Bot:   botstrat, botdbreset, nearbystrategies");
#endif
}

// =============================================================================
// Command table
// =============================================================================

std::vector<ModuleChatCommand>* ExtraCommandsModule::GetCommandTable()
{
    if (commandTable.empty())
    {
        const ExtraCommandsModuleConfig* cfg =
            static_cast<const ExtraCommandsModuleConfig*>(GetConfig());

        uint32 sec = cfg ? cfg->securityLevel : SEC_GAMEMASTER;

        commandTable =
        {
            // Guild data commands
            { "guildmotd",    [this](WorldSession* s, const std::string& a) { return HandleGuildMotdCommand(s, a);    }, sec },
            { "guildinfo",    [this](WorldSession* s, const std::string& a) { return HandleGuildInfoCommand(s, a);    }, sec },
            { "guildpnote",   [this](WorldSession* s, const std::string& a) { return HandleGuildPNoteCommand(s, a);   }, sec },
            { "guildoffnote", [this](WorldSession* s, const std::string& a) { return HandleGuildOFFNoteCommand(s, a); }, sec },
            { "guildlist",    [this](WorldSession* s, const std::string& a) { return HandleGuildListCommand(s, a);    }, sec },
            { "guildcount",   [this](WorldSession* s, const std::string& a) { return HandleGuildCountCommand(s, a);   }, sec },
            { "guildempty",   [this](WorldSession* s, const std::string& a) { return HandleGuildEmptyCommand(s, a);   }, sec },

            // Guild flavor
            { "guildflavor",  [this](WorldSession* s, const std::string& a) { return HandleGuildFlavorCommand(s, a);  }, sec },

            // Bot strategy commands
            { "botstrat",          [this](WorldSession* s, const std::string& a) { return HandleBotStratCommand(s, a);         }, sec },
            { "botdbreset",        [this](WorldSession* s, const std::string& a) { return HandleBotDbResetCommand(s, a);       }, sec },
            { "nearbystrategies",  [this](WorldSession* s, const std::string& a) { return HandleNearbyStrategiesCommand(s, a); }, sec },
        };
    }
    return &commandTable;
}

// =============================================================================
// Helpers
// =============================================================================

void ExtraCommandsModule::SendMsg(WorldSession* session, const std::string& msg) const
{
    if (session && session->GetPlayer())
        ChatHandler(session).SendSysMessage(msg.c_str());
}

std::string ExtraCommandsModule::NormalizeName(const std::string& name) const
{
    if (name.empty()) return name;
    std::string out = name;
    out[0] = toupper(out[0]);
    for (size_t i = 1; i < out.size(); ++i)
        out[i] = tolower(out[i]);
    return out;
}

std::string ExtraCommandsModule::ExtractQuoted(const std::string& input, std::string& remainder) const
{
    size_t start = input.find('"');
    if (start == std::string::npos) return "";

    size_t end = input.find('"', start + 1);
    if (end == std::string::npos) return "";

    std::string quoted = input.substr(start + 1, end - start - 1);

    remainder = (end + 1 < input.size()) ? input.substr(end + 1) : "";
    size_t nonspace = remainder.find_first_not_of(' ');
    remainder = (nonspace != std::string::npos) ? remainder.substr(nonspace) : "";

    return quoted;
}

std::string ExtraCommandsModule::FormatStratList(std::list<std::string_view> strats) const
{
    std::ostringstream ss;
    for (auto& sv : strats) ss << sv << " ";
    std::string r = ss.str();
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r.empty() ? "(none)" : r;
}

// =============================================================================
// Guild Commands
// =============================================================================

// .ec guildmotd "<guildname>" <message>
bool ExtraCommandsModule::HandleGuildMotdCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildmotd \"<guildname>\" <message>"); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty() || remainder.empty()) { SendMsg(session, "Usage: .ec guildmotd \"<guildname>\" <message>"); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    guild->SetMOTD(remainder);
    guild->BroadcastEvent(GE_MOTD, remainder.c_str());
    SendMsg(session, "MOTD for [" + guildName + "] set to: " + remainder);
    return true;
}

// .ec guildinfo "<guildname>" <text>
bool ExtraCommandsModule::HandleGuildInfoCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildinfo \"<guildname>\" <text>"); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty() || remainder.empty()) { SendMsg(session, "Usage: .ec guildinfo \"<guildname>\" <text>"); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    guild->SetGINFO(remainder);
    SendMsg(session, "Info for [" + guildName + "] updated.");
    return true;
}

// .ec guildpnote <playername> <note>
bool ExtraCommandsModule::HandleGuildPNoteCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildpnote <playername> <note>"); return false; }

    size_t sp = args.find(' ');
    if (sp == std::string::npos) { SendMsg(session, "Usage: .ec guildpnote <playername> <note>"); return false; }

    std::string playerName = NormalizeName(args.substr(0, sp));
    std::string note       = args.substr(sp + 1);
    if (note.empty()) { SendMsg(session, "Usage: .ec guildpnote <playername> <note>"); return false; }

    Player*    player     = sObjectMgr.GetPlayer(playerName.c_str());
    ObjectGuid playerGuid;
    uint32     guildId    = 0;

    if (player) { playerGuid = player->GetObjectGuid(); guildId = player->GetGuildId(); }
    else
    {
        playerGuid = sObjectMgr.GetPlayerGuidByName(playerName);
        if (playerGuid.IsEmpty()) { SendMsg(session, "Player not found: " + playerName); return false; }
        guildId = Player::GetGuildIdFromDB(playerGuid);
    }

    if (!guildId) { SendMsg(session, playerName + " is not in a guild."); return false; }

    Guild* guild = sGuildMgr.GetGuildById(guildId);
    if (!guild) { SendMsg(session, "Could not find guild for " + playerName); return false; }

    MemberSlot* member = guild->GetMemberSlot(playerGuid);
    if (!member) { SendMsg(session, "Could not find guild member slot for " + playerName); return false; }

    member->SetPNOTE(note);
    SendMsg(session, "Public note for " + playerName + " set to: " + note);
    return true;
}

// .ec guildoffnote <playername> <note>
bool ExtraCommandsModule::HandleGuildOFFNoteCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildoffnote <playername> <note>"); return false; }

    size_t sp = args.find(' ');
    if (sp == std::string::npos) { SendMsg(session, "Usage: .ec guildoffnote <playername> <note>"); return false; }

    std::string playerName = NormalizeName(args.substr(0, sp));
    std::string note       = args.substr(sp + 1);
    if (note.empty()) { SendMsg(session, "Usage: .ec guildoffnote <playername> <note>"); return false; }

    Player*    player     = sObjectMgr.GetPlayer(playerName.c_str());
    ObjectGuid playerGuid;
    uint32     guildId    = 0;

    if (player) { playerGuid = player->GetObjectGuid(); guildId = player->GetGuildId(); }
    else
    {
        playerGuid = sObjectMgr.GetPlayerGuidByName(playerName);
        if (playerGuid.IsEmpty()) { SendMsg(session, "Player not found: " + playerName); return false; }
        guildId = Player::GetGuildIdFromDB(playerGuid);
    }

    if (!guildId) { SendMsg(session, playerName + " is not in a guild."); return false; }

    Guild* guild = sGuildMgr.GetGuildById(guildId);
    if (!guild) { SendMsg(session, "Could not find guild for " + playerName); return false; }

    MemberSlot* member = guild->GetMemberSlot(playerGuid);
    if (!member) { SendMsg(session, "Could not find guild member slot for " + playerName); return false; }

    member->SetOFFNOTE(note);
    SendMsg(session, "Officer note for " + playerName + " set to: " + note);
    return true;
}

// .ec guildlist "<guildname>"
// NAME|RANKID|LEVEL|CLASS|ONLINE|PNOTE|OFFNOTE
bool ExtraCommandsModule::HandleGuildListCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildlist \"<guildname>\""); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty()) { SendMsg(session, "Guild name must be in quotes. Usage: .ec guildlist \"<guildname>\""); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    ChatHandler handler(session);
    handler.PSendSysMessage("GUILD|%s", guild->GetName().c_str());
    handler.SendSysMessage("NAME|RANKID|LEVEL|CLASS|ONLINE|PNOTE|OFFNOTE");

    Guild::MemberList const& members = guild->GetMembers();
    for (auto const& kv : members)
    {
        MemberSlot const& m = kv.second;
        Player*           p = sObjectMgr.GetPlayer(m.guid);
        uint32 lvl = p ? p->GetLevel()   : m.RankLevel;
        uint32 cls = p ? p->getClass()   : m.RankClass;
        bool   on  = (p != nullptr);

        handler.PSendSysMessage("%s|%u|%u|%u|%d|%s|%s",
            m.Name.c_str(), m.RankId, lvl, cls, on ? 1 : 0,
            m.Pnote.c_str(), m.OFFnote.c_str());
    }

    handler.SendSysMessage("END|GUILDLIST");
    return true;
}

// .ec guildcount "<guildname>"
// Shows member total, online count, average level.
bool ExtraCommandsModule::HandleGuildCountCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildcount \"<guildname>\""); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty()) { SendMsg(session, "Guild name must be in quotes."); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    Guild::MemberList const& members = guild->GetMembers();

    int total = 0, online = 0;
    uint64 levelSum = 0;

    for (auto const& kv : members)
    {
        MemberSlot const& m = kv.second;
        Player* p = sObjectMgr.GetPlayer(m.guid);
        uint32 lvl = p ? p->GetLevel() : m.RankLevel;
        levelSum += lvl;
        ++total;
        if (p) ++online;
    }

    float avgLevel = total > 0 ? (float)levelSum / total : 0.0f;

    ChatHandler handler(session);
    handler.PSendSysMessage("Guild: %s", guildName.c_str());
    handler.PSendSysMessage("Members: %d total | %d online | avg level %.1f", total, online, avgLevel);
    return true;
}

// .ec guildempty
// Lists guilds that have no online bots.
bool ExtraCommandsModule::HandleGuildEmptyCommand(WorldSession* session, const std::string& /*args*/)
{
    // Build a map of online member counts per guild from active sessions
    std::map<uint32, int> onlinePerGuild;
    SessionMap const& sessions = sWorld.GetAllSessions();
    for (auto const& pair : sessions)
    {
        WorldSession* ws = pair.second;
        if (!ws) continue;
        Player* p = ws->GetPlayer();
        if (!p) continue;
        uint32 gid = p->GetGuildId();
        if (gid) onlinePerGuild[gid]++;
    }

    // Query all guilds from DB and report those with 0 online
    auto results = CharacterDatabase.PQuery(
        "SELECT guildid, name FROM guild ORDER BY name ASC");

    if (!results)
    {
        SendMsg(session, "No guilds found.");
        return true;
    }

    ChatHandler handler(session);
    handler.SendSysMessage("Guilds with no online members:");
    int count = 0;

    do
    {
        Field* fields  = results->Fetch();
        uint32 guildId = fields[0].GetUInt32();
        std::string name = fields[1].GetString();

        if (onlinePerGuild.find(guildId) == onlinePerGuild.end())
        {
            handler.PSendSysMessage("  %s", name.c_str());
            ++count;
        }
    } while (results->NextRow());

    if (count == 0)
        handler.SendSysMessage("  (none — all guilds have at least one online member)");
    else
        handler.PSendSysMessage("Total: %d guild(s)", count);

    return true;
}

// =============================================================================
// Guild Flavor Commands
// =============================================================================

// .ec guildflavor "<guildname>"          — show current DB override for a sample member
// .ec guildflavor "<guildname>" <flavor> — write flavor strategies to all guild members
bool ExtraCommandsModule::HandleGuildFlavorCommand(WorldSession* session, const std::string& args)
{
    if (args.empty())
    {
        SendMsg(session, "Usage: .ec guildflavor \"<guildname>\" [leveling|quest|pvp|farming|default]");
        return false;
    }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty())
    {
        SendMsg(session, "Guild name must be in quotes.");
        return false;
    }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    Guild::MemberList const& members = guild->GetMembers();
    if (members.empty()) { SendMsg(session, "Guild has no members."); return false; }

    ChatHandler handler(session);

    // ── Show mode ──────────────────────────────────────────────────────────
    if (remainder.empty())
    {
        // Query DB store for the first member to show current overrides
        ObjectGuid sampleGuid = members.begin()->second.guid;
        uint64 rawGuid = sampleGuid.GetRawValue();

        auto results = CharacterDatabase.PQuery(
            "SELECT `key`, `value` FROM ai_playerbot_db_store "
            "WHERE guid = '%lu' AND preset = 'default'", rawGuid);

        handler.PSendSysMessage("Guild: %s (%u members)", guildName.c_str(), (uint32)members.size());

        if (!results)
        {
            handler.SendSysMessage("Flavor: default (no DB overrides — using global config)");
            return true;
        }

        handler.SendSysMessage("Flavor: custom (DB overrides set)");
        do
        {
            Field* f = results->Fetch();
            handler.PSendSysMessage("  %s: %s", f[0].GetString(), f[1].GetString());
        } while (results->NextRow());

        return true;
    }

    // ── Set mode ───────────────────────────────────────────────────────────
    std::string flavor = remainder;
    // lowercase
    for (char& c : flavor) c = tolower(c);

    // "default" = clear all rows, bots fall back to global config
    bool isDefault = (flavor == "default");

    if (!isDefault && s_flavorProfiles.find(flavor) == s_flavorProfiles.end())
    {
        SendMsg(session, "Unknown flavor '" + flavor + "'. Valid: leveling, quest, pvp, farming, default");
        return false;
    }

    int applied = 0;
    for (auto const& kv : members)
    {
        uint64 rawGuid = kv.second.guid.GetRawValue();

        // Clear existing overrides for this bot
        CharacterDatabase.PExecute(
            "DELETE FROM ai_playerbot_db_store WHERE guid = '%lu' AND preset = 'default'", rawGuid);

        if (!isDefault)
        {
            const FlavorProfile& fp = s_flavorProfiles.at(flavor);

            CharacterDatabase.PExecute(
                "INSERT INTO ai_playerbot_db_store (guid, preset, `key`, value) VALUES "
                "('%lu', 'default', 'co', '%s')", rawGuid, fp.co.c_str());

            CharacterDatabase.PExecute(
                "INSERT INTO ai_playerbot_db_store (guid, preset, `key`, value) VALUES "
                "('%lu', 'default', 'nc', '%s')", rawGuid, fp.nc.c_str());

            if (!fp.react.empty())
                CharacterDatabase.PExecute(
                    "INSERT INTO ai_playerbot_db_store (guid, preset, `key`, value) VALUES "
                    "('%lu', 'default', 'react', '%s')", rawGuid, fp.react.c_str());
        }

#ifdef HAVE_PLAYERBOTS
        // Apply live to any online bots immediately — no relog required
        Player* bot = sObjectMgr.GetPlayer(kv.second.guid);
        if (bot)
        {
            PlayerbotAI* ai = bot->GetPlayerbotAI();
            if (ai)
                ai->ResetStrategies(true); // true = reload from DB store we just wrote
        }
#endif
        ++applied;
    }

    handler.PSendSysMessage("Guild [%s] flavor set to '%s' — %d members updated.",
        guildName.c_str(), isDefault ? "default" : flavor.c_str(), applied);
    return true;
}

// =============================================================================
// Bot Strategy Commands
// =============================================================================

// .ec botstrat <name>
//   — show active co/nc/react/dead strategies
// .ec botstrat <name> co|nc|react|dead <strategies>
//   — change strategies live
bool ExtraCommandsModule::HandleBotStratCommand(WorldSession* session, const std::string& args)
{
#ifndef HAVE_PLAYERBOTS
    SendMsg(session, "Server not built with playerbots.");
    return false;
#else
    if (args.empty())
    {
        SendMsg(session, "Usage: .ec botstrat <name> [co|nc|react|dead <strategies>]");
        return false;
    }

    size_t sp = args.find(' ');
    std::string name = NormalizeName(sp == std::string::npos ? args : args.substr(0, sp));
    std::string rest = sp == std::string::npos ? "" : args.substr(sp + 1);

    Player* bot = sObjectMgr.GetPlayer(name.c_str());
    if (!bot) { SendMsg(session, "Bot not found online: " + name); return false; }

    PlayerbotAI* ai = bot->GetPlayerbotAI();
    if (!ai) { SendMsg(session, name + " is not a bot."); return false; }

    ChatHandler handler(session);

    if (rest.empty())
    {
        // Show mode
        handler.PSendSysMessage("Bot: %s", name.c_str());
        handler.PSendSysMessage("  co:    %s", FormatStratList(ai->GetStrategies(BotState::BOT_STATE_COMBAT)).c_str());
        handler.PSendSysMessage("  nc:    %s", FormatStratList(ai->GetStrategies(BotState::BOT_STATE_NON_COMBAT)).c_str());
        handler.PSendSysMessage("  react: %s", FormatStratList(ai->GetStrategies(BotState::BOT_STATE_REACTION)).c_str());
        handler.PSendSysMessage("  dead:  %s", FormatStratList(ai->GetStrategies(BotState::BOT_STATE_DEAD)).c_str());
        return true;
    }

    // Set mode: rest = "co +dps,-threat"
    size_t sp2 = rest.find(' ');
    if (sp2 == std::string::npos)
    {
        SendMsg(session, "Usage: .ec botstrat <name> co|nc|react|dead <strategies>");
        return false;
    }

    std::string key    = rest.substr(0, sp2);
    std::string strats = rest.substr(sp2 + 1);

    BotState state;
    if      (key == "co")    state = BotState::BOT_STATE_COMBAT;
    else if (key == "nc")    state = BotState::BOT_STATE_NON_COMBAT;
    else if (key == "react") state = BotState::BOT_STATE_REACTION;
    else if (key == "dead")  state = BotState::BOT_STATE_DEAD;
    else
    {
        SendMsg(session, "Unknown key '" + key + "'. Use: co, nc, react, dead");
        return false;
    }

    ai->ChangeStrategy(strats, state);
    handler.PSendSysMessage("Updated %s [%s]: %s", name.c_str(), key.c_str(), strats.c_str());
    return true;
#endif
}

// .ec botdbreset <name>
// Wipes ai_playerbot_db_store for a bot and resets live strategies to defaults.
bool ExtraCommandsModule::HandleBotDbResetCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec botdbreset <name>"); return false; }

    std::string name = args;
    while (!name.empty() && name.back() == ' ') name.pop_back();
    name = NormalizeName(name);

    ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name);
    if (guid.IsEmpty()) { SendMsg(session, "Player not found: " + name); return false; }

    CharacterDatabase.PExecute(
        "DELETE FROM ai_playerbot_db_store WHERE guid = '%lu'", guid.GetRawValue());

#ifdef HAVE_PLAYERBOTS
    Player* bot = sObjectMgr.GetPlayer(guid);
    if (bot)
    {
        PlayerbotAI* ai = bot->GetPlayerbotAI();
        if (ai) ai->ResetStrategies(false); // false = don't reload DB (we just cleared it)
    }
#endif

    SendMsg(session, "DB store cleared for " + name + ". Bot now uses global config defaults.");
    return true;
}

// .ec nearbystrategies [radius]
// Lists all bots within radius yards with their co/nc strategies.
bool ExtraCommandsModule::HandleNearbyStrategiesCommand(WorldSession* session, const std::string& args)
{
#ifndef HAVE_PLAYERBOTS
    SendMsg(session, "Server not built with playerbots.");
    return false;
#else
    float radius = 30.0f;
    if (!args.empty())
    {
        try { radius = std::stof(args); }
        catch (...) { SendMsg(session, "Invalid radius. Usage: .ec nearbystrategies [radius]"); return false; }
    }

    Player* gm = session->GetPlayer();
    if (!gm) return false;

    ChatHandler handler(session);
    handler.PSendSysMessage("Bots within %.0f yards:", radius);

    int count = 0;
    SessionMap const& sessions = sWorld.GetAllSessions();
    for (auto const& pair : sessions)
    {
        WorldSession* ws = pair.second;
        if (!ws) continue;
        Player* p = ws->GetPlayer();
        if (!p || p == gm) continue;
        if (p->GetMapId() != gm->GetMapId()) continue;
        if (gm->GetDistance(p) > radius) continue;

        PlayerbotAI* ai = p->GetPlayerbotAI();
        if (!ai) continue;

        std::string co = FormatStratList(ai->GetStrategies(BotState::BOT_STATE_COMBAT));
        std::string nc = FormatStratList(ai->GetStrategies(BotState::BOT_STATE_NON_COMBAT));

        handler.PSendSysMessage("[%s] co: %s | nc: %s", p->GetName(), co.c_str(), nc.c_str());
        ++count;
    }

    if (count == 0)
        handler.SendSysMessage("No bots found nearby.");
    else
        handler.PSendSysMessage("Total: %d bots", count);

    return true;
#endif
}

} // namespace cmangos_module
