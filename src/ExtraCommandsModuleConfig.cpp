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
    enabled       = config.GetBoolDefault("ExtraCommands.Enabled",       true);
    securityLevel = config.GetIntDefault ("ExtraCommands.SecurityLevel", SEC_GAMEMASTER);
    return true;
}

}
