#pragma once
#include "Module.h"

namespace cmangos_module
{

class ExtraCommandsModuleConfig : public ModuleConfig
{
public:
    ExtraCommandsModuleConfig();
    bool OnLoad() override;

    bool enabled;
    uint32 securityLevel;   // Minimum security level required for all .ec commands
};

}
