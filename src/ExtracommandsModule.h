#pragma once
#include "Module.h"
#include "ExtraCommandsModuleConfig.h"
#include <vector>
#include <string>
#include <list>
#include <string_view>

class WorldSession;

#ifdef HAVE_PLAYERBOTS
#include "playerbot/PlayerbotAI.h"
#endif

namespace cmangos_module
{

class ExtracommandsModule : public Module
{
public:
    ExtracommandsModule();

    // Module identity
    const char* GetChatCommandPrefix() const override { return "ec"; }
    std::vector<ModuleChatCommand>* GetCommandTable() override;

    // Module hooks
    void OnInitialize() override;

private:
    // -------------------------------------------------------------------------
    // Guild data commands
    //   .ec guildmotd     "<guildname>" <message>
    //   .ec guildinfo     "<guildname>" <text>
    //   .ec guildpnote    <playername> <note>
    //   .ec guildoffnote  <playername> <note>
    //   .ec guildlist     "<guildname>"
    //   .ec guildcount    "<guildname>"
    //   .ec guildempty
    // -------------------------------------------------------------------------
    bool HandleGuildMotdCommand    (WorldSession* session, const std::string& args);
    bool HandleGuildInfoCommand    (WorldSession* session, const std::string& args);
    bool HandleGuildPNoteCommand   (WorldSession* session, const std::string& args);
    bool HandleGuildOFFNoteCommand (WorldSession* session, const std::string& args);
    bool HandleGuildListCommand    (WorldSession* session, const std::string& args);
    bool HandleGuildCountCommand   (WorldSession* session, const std::string& args);
    bool HandleGuildEmptyCommand   (WorldSession* session, const std::string& args);

    // -------------------------------------------------------------------------
    // Guild flavor commands (guild flavor system — ai_playerbot_db_store)
    //   .ec guildflavor   "<guildname>"
    //   .ec guildflavor   "<guildname>" <flavor>
    // -------------------------------------------------------------------------
    bool HandleGuildFlavorCommand  (WorldSession* session, const std::string& args);

    // -------------------------------------------------------------------------
    // Bot strategy commands (requires HAVE_PLAYERBOTS)
    //   .ec botstrat          <name>
    //   .ec botstrat          <name> co|nc|react|dead <strategies>
    //   .ec botdbreset        <name>
    //   .ec nearbystrategies  [radius]
    // -------------------------------------------------------------------------
    bool HandleBotStratCommand         (WorldSession* session, const std::string& args);
    bool HandleBotDbResetCommand       (WorldSession* session, const std::string& args);
    bool HandleNearbyStrategiesCommand (WorldSession* session, const std::string& args);

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    std::string ExtractQuoted      (const std::string& input, std::string& remainder) const;
    std::string NormalizeName      (const std::string& name) const;
    std::string Trim              (const std::string& input) const;
    std::string UnquoteArgument   (const std::string& input) const;
    void        SendMsg            (WorldSession* session, const std::string& msg) const;
    std::string FormatStratList    (std::list<std::string_view> strats) const;

    std::vector<ModuleChatCommand> commandTable;
};

}
