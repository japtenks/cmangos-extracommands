#include "ExtracommandsModule.h"
#include "ExtraCommandsModuleConfig.h"

// CMaNGOS core includes
#include "Globals/ObjectMgr.h"
#include "Globals/ObjectAccessor.h"
#include "Guilds/GuildMgr.h"
#include "Guilds/Guild.h"
#include "Entities/Player.h"
#include "World/World.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"
#include "Database/DatabaseEnv.h"

#include <sstream>
#include <map>
#include <set>
#include <cctype>

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

ExtracommandsModule::ExtracommandsModule()
    : Module("ExtracommandsModule", new ExtraCommandsModuleConfig())
{
}

void ExtracommandsModule::OnInitialize()
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

std::vector<ModuleChatCommand>* ExtracommandsModule::GetCommandTable()
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

void ExtracommandsModule::SendMsg(WorldSession* session, const std::string& msg) const
{
    if (session && session->GetPlayer())
        ChatHandler(session).SendSysMessage(msg.c_str());
}

std::string ExtracommandsModule::NormalizeName(const std::string& name) const
{
    if (name.empty()) return name;
    std::string out = name;
    out[0] = toupper(out[0]);
    for (size_t i = 1; i < out.size(); ++i)
        out[i] = tolower(out[i]);
    return out;
}

std::string ExtracommandsModule::Trim(const std::string& input) const
{
    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

std::string ExtracommandsModule::ExtractQuoted(const std::string& input, std::string& remainder) const
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

std::string ExtracommandsModule::UnquoteArgument(const std::string& input) const
{
    std::string value = Trim(input);
    if (value.size() < 2 || value.front() != '"' || value.back() != '"')
        return value;

    std::string unquoted;
    unquoted.reserve(value.size() - 2);

    for (size_t i = 1; i + 1 < value.size(); ++i)
    {
        if (value[i] == '\\' && i + 2 < value.size())
        {
            ++i;
            unquoted.push_back(value[i]);
            continue;
        }

        unquoted.push_back(value[i]);
    }

    return unquoted;
}

std::string ExtracommandsModule::FormatStratList(std::list<std::string_view> strats) const
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
bool ExtracommandsModule::HandleGuildMotdCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildmotd \"<guildname>\" <message>"); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    std::string motdArg = Trim(remainder);
    if (guildName.empty() || motdArg.empty()) { SendMsg(session, "Usage: .ec guildmotd \"<guildname>\" <message>"); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    std::string motd = UnquoteArgument(motdArg);
    guild->SetMOTD(motd);
    guild->BroadcastEvent(GE_MOTD, motd.c_str());
    SendMsg(session, motd.empty() ? "MOTD for [" + guildName + "] cleared."
                                  : "MOTD for [" + guildName + "] set to: " + motd);
    return true;
}

// .ec guildinfo "<guildname>" <text>
bool ExtracommandsModule::HandleGuildInfoCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildinfo \"<guildname>\" <text>"); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    std::string infoArg = Trim(remainder);
    if (guildName.empty() || infoArg.empty()) { SendMsg(session, "Usage: .ec guildinfo \"<guildname>\" <text>"); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    std::string info = UnquoteArgument(infoArg);
    guild->SetGINFO(info);
    SendMsg(session, info.empty() ? "Info for [" + guildName + "] cleared."
                                  : "Info for [" + guildName + "] updated.");
    return true;
}

// .ec guildpnote <playername> <note>
bool ExtracommandsModule::HandleGuildPNoteCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildpnote <playername> <note>"); return false; }

    size_t sp = args.find(' ');
    if (sp == std::string::npos) { SendMsg(session, "Usage: .ec guildpnote <playername> <note>"); return false; }

    std::string playerName = NormalizeName(args.substr(0, sp));
    std::string rawNote    = Trim(args.substr(sp + 1));
    if (rawNote.empty()) { SendMsg(session, "Usage: .ec guildpnote <playername> <note>"); return false; }
    std::string note       = UnquoteArgument(rawNote);

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
    SendMsg(session, note.empty() ? "Public note for " + playerName + " cleared."
                                  : "Public note for " + playerName + " set to: " + note);
    return true;
}

// .ec guildoffnote <playername> <note>
bool ExtracommandsModule::HandleGuildOFFNoteCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildoffnote <playername> <note>"); return false; }

    size_t sp = args.find(' ');
    if (sp == std::string::npos) { SendMsg(session, "Usage: .ec guildoffnote <playername> <note>"); return false; }

    std::string playerName = NormalizeName(args.substr(0, sp));
    std::string rawNote    = Trim(args.substr(sp + 1));
    if (rawNote.empty()) { SendMsg(session, "Usage: .ec guildoffnote <playername> <note>"); return false; }
    std::string note       = UnquoteArgument(rawNote);

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
    SendMsg(session, note.empty() ? "Officer note for " + playerName + " cleared."
                                  : "Officer note for " + playerName + " set to: " + note);
    return true;
}

// .ec guildlist "<guildname>"
// NAME|RANKID|LEVEL|CLASS|ONLINE|PNOTE|OFFNOTE
bool ExtracommandsModule::HandleGuildListCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildlist \"<guildname>\""); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty()) { SendMsg(session, "Guild name must be in quotes. Usage: .ec guildlist \"<guildname>\""); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    if (!session)
        return true;

    ChatHandler handler(session);
    handler.PSendSysMessage("GUILD|%s", guild->GetName().c_str());
    handler.SendSysMessage("NAME|RANKID|LEVEL|CLASS|ONLINE|PNOTE|OFFNOTE");

    auto results = CharacterDatabase.PQuery(
        "SELECT gm.guid, gm.rank, gm.pnote, gm.offnote, c.name, c.level, c.class "
        "FROM guild_member gm JOIN characters c ON c.guid = gm.guid "
        "WHERE gm.guildid = %u ORDER BY gm.rank, c.name", guild->GetId());

    if (results)
    {
        do
        {
            Field* f = results->Fetch();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, f[0].GetUInt32());
            Player* p = sObjectMgr.GetPlayer(guid);
            uint32 rankId = f[1].GetUInt32();
            std::string pnote  = f[2].GetString();
            std::string offnote = f[3].GetString();
            std::string name   = f[4].GetString();
            uint32 lvl = p ? p->GetLevel()   : f[5].GetUInt32();
            uint32 cls = p ? p->getClass()   : f[6].GetUInt32();
            bool on = (p != nullptr);

            handler.PSendSysMessage("%s|%u|%u|%u|%d|%s|%s",
                name.c_str(), rankId, lvl, cls, on ? 1 : 0,
                pnote.c_str(), offnote.c_str());
        } while (results->NextRow());
    }

    handler.SendSysMessage("END|GUILDLIST");
    return true;
}

// .ec guildcount "<guildname>"
// Shows member total, online count, average level.
bool ExtracommandsModule::HandleGuildCountCommand(WorldSession* session, const std::string& args)
{
    if (args.empty()) { SendMsg(session, "Usage: .ec guildcount \"<guildname>\""); return false; }

    std::string remainder;
    std::string guildName = ExtractQuoted(args, remainder);
    if (guildName.empty()) { SendMsg(session, "Guild name must be in quotes."); return false; }

    Guild* guild = sGuildMgr.GetGuildByName(guildName);
    if (!guild) { SendMsg(session, "Guild not found: " + guildName); return false; }

    uint32 total = guild->GetMemberSize();

    auto results = CharacterDatabase.PQuery(
        "SELECT AVG(c.level), SUM(c.online) "
        "FROM guild_member gm JOIN characters c ON c.guid = gm.guid "
        "WHERE gm.guildid = %u", guild->GetId());

    float avgLevel = 0.0f;
    int online = 0;
    if (results)
    {
        Field* f = results->Fetch();
        avgLevel = f[0].GetFloat();
        online   = f[1].GetUInt32();
    }

    if (!session)
        return true;

    ChatHandler handler(session);
    handler.PSendSysMessage("Guild: %s", guildName.c_str());
    handler.PSendSysMessage("Members: %u total | %d online | avg level %.1f", total, online, avgLevel);
    return true;
}

// .ec guildempty
// Lists guilds that have no online bots.
bool ExtracommandsModule::HandleGuildEmptyCommand(WorldSession* session, const std::string& /*args*/)
{
    // Build set of guild IDs that have at least one online member
    std::set<uint32> onlineGuildIds;
    auto& players = sObjectAccessor.GetPlayers();
    for (auto const& pair : players)
    {
        Player* p = pair.second;
        if (!p) continue;
        uint32 gid = p->GetGuildId();
        if (gid) onlineGuildIds.insert(gid);
    }

    // Query all guilds from DB and report those with 0 online
    auto results = CharacterDatabase.PQuery(
        "SELECT guildid, name FROM guild ORDER BY name ASC");

    if (!results)
    {
        SendMsg(session, "No guilds found.");
        return true;
    }

    if (!session)
        return true;

    ChatHandler handler(session);
    handler.SendSysMessage("Guilds with no online members:");
    int count = 0;

    do
    {
        Field* fields  = results->Fetch();
        uint32 guildId = fields[0].GetUInt32();
        std::string name = fields[1].GetString();

        if (onlineGuildIds.find(guildId) == onlineGuildIds.end())
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
bool ExtracommandsModule::HandleGuildFlavorCommand(WorldSession* session, const std::string& args)
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

    uint32 memberCount = guild->GetMemberSize();
    if (memberCount == 0) { SendMsg(session, "Guild has no members."); return false; }

    // ── Show mode ──────────────────────────────────────────────────────────
    if (remainder.empty())
    {
        if (!session)
            return true;

        ChatHandler handler(session);

        // Query DB store for the first member to show current overrides
        auto firstMember = CharacterDatabase.PQuery(
            "SELECT guid FROM guild_member WHERE guildid = %u LIMIT 1", guild->GetId());
        if (!firstMember) { SendMsg(session, "No members found in DB."); return false; }
        uint64 rawGuid = firstMember->Fetch()[0].GetUInt64();

        auto results = CharacterDatabase.PQuery(
            "SELECT `key`, `value` FROM ai_playerbot_db_store "
            "WHERE guid = '%lu' AND preset = 'default'", rawGuid);

        handler.PSendSysMessage("Guild: %s (%u members)", guildName.c_str(), memberCount);

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

    auto memberResults = CharacterDatabase.PQuery(
        "SELECT guid FROM guild_member WHERE guildid = %u", guild->GetId());

    int applied = 0;
    if (memberResults)
    {
        do
        {
            uint64 rawGuid = memberResults->Fetch()[0].GetUInt64();
            ObjectGuid guid(HIGHGUID_PLAYER, (uint32)rawGuid);

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
            Player* bot = sObjectMgr.GetPlayer(guid);
            if (bot)
            {
                PlayerbotAI* ai = bot->GetPlayerbotAI();
                if (ai)
                    ai->ResetStrategies(true); // true = reload from DB store we just wrote
            }
#endif
            ++applied;
        } while (memberResults->NextRow());
    }

    if (session)
    {
        ChatHandler handler(session);
        handler.PSendSysMessage("Guild [%s] flavor set to '%s' — %d members updated.",
            guildName.c_str(), isDefault ? "default" : flavor.c_str(), applied);
    }
    return true;
}

// =============================================================================
// Bot Strategy Commands
// =============================================================================

// .ec botstrat <name>
//   — show active co/nc/react/dead strategies
// .ec botstrat <name> co|nc|react|dead <strategies>
//   — change strategies live
bool ExtracommandsModule::HandleBotStratCommand(WorldSession* session, const std::string& args)
{
#ifndef HAVE_PLAYERBOTS
    SendMsg(session, "Server not built with playerbots.");
    return false;
#else
    if (!session)
        return false;

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
bool ExtracommandsModule::HandleBotDbResetCommand(WorldSession* session, const std::string& args)
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
bool ExtracommandsModule::HandleNearbyStrategiesCommand(WorldSession* session, const std::string& args)
{
#ifndef HAVE_PLAYERBOTS
    SendMsg(session, "Server not built with playerbots.");
    return false;
#else
    if (!session)
        return false;

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
    auto& players = sObjectAccessor.GetPlayers();
    for (auto const& pair : players)
    {
        Player* p = pair.second;
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
