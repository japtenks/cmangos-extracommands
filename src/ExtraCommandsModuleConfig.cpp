#include "ExtraCommandsModuleConfig.h"

namespace cmangos_module
{

ExtraCommandsModuleConfig::ExtraCommandsModuleConfig()
    : ModuleConfig("extracommands.conf")
    , enabled(true)
    , securityLevel(SEC_GAMEMASTER)
{
}

bool ExtraCommandsModuleConfig::OnLoad()
{
    enabled      = GetBoolDefault("ExtraCommands.Enabled",       true);
    securityLevel = GetIntDefault ("ExtraCommands.SecurityLevel", SEC_GAMEMASTER);
    return true;
}

}
