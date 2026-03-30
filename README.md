# ExtraCommands

ExtraCommands adds extended GM and admin commands to CMaNGOS servers under the `.ec` prefix. It fills gaps in the standard command set — particularly for managing bot guilds, inspecting live bot AI strategies, and applying guild flavor profiles without relogging or editing config files manually.

The module requires no new database tables. It works against existing CMaNGOS and Playerbot tables (`guild`, `guild_member`, `ai_playerbot_db_store`).

## Commands

All commands require the security level configured in `extracommands.conf` (default: GameMaster).

### Guild Data
| Command | Description |
|---------|-------------|
| `.ec guildmotd "<name>" <message>` | Sets guild MOTD and broadcasts live to online members |
| `.ec guildinfo "<name>" <text>` | Sets guild info tab text |
| `.ec guildpnote <player> <note>` | Sets public note for a guild member (works offline) |
| `.ec guildoffnote <player> <note>` | Sets officer note for a guild member (works offline) |
| `.ec guildlist "<name>"` | Lists all members: `NAME\|RANKID\|LEVEL\|CLASS\|ONLINE\|PNOTE\|OFFNOTE` |
| `.ec guildcount "<name>"` | Shows total members, online count, and average level |
| `.ec guildempty` | Lists all guilds that currently have no online members |

### Guild Flavor (AI Playerbot strategy profiles)
| Command | Description |
|---------|-------------|
| `.ec guildflavor "<name>"` | Shows current strategy overrides for the guild |
| `.ec guildflavor "<name>" <flavor>` | Sets flavor for all members. Online bots update immediately. |

Valid flavors: `leveling`, `quest`, `pvp`, `farming`, `default`
`default` clears all overrides — bots fall back to global `aiplayerbot.conf` config.

### Bot Strategies (requires `BUILD_PLAYERBOTS=ON`)
| Command | Description |
|---------|-------------|
| `.ec botstrat <name>` | Shows active `co`/`nc`/`react`/`dead` strategies for an online bot |
| `.ec botstrat <name> co\|nc\|react\|dead <strategies>` | Changes strategies live without whispering the bot |
| `.ec botdbreset <name>` | Clears `ai_playerbot_db_store` for a bot, resets to global defaults |
| `.ec nearbystrategies [radius]` | Lists all bots within radius yards with their strategies (default: 30yd) |

# Available Cores
Classic, TBC, WotLK

# How to install
1. Follow the instructions in https://github.com/japtenks/cmangos-modules?tab=readme-ov-file#how-to-install
2. Enable the `BUILD_MODULE_EXTRACOMMANDS` flag in cmake and run cmake. The module will be installed in `src/modules/extracommands`
3. Copy the configuration file from `src/modules/extracommands/src/extracommands.conf.dist.in` to the folder where your `mangosd` executable is and rename it to `extracommands.conf`
4. Edit `extracommands.conf` to set the security level and enable/disable the module as needed.
5. No database changes are required — the module uses existing CMaNGOS and Playerbot tables.

# How to uninstall
1. Remove the `BUILD_MODULE_EXTRACOMMANDS` flag from your cmake configuration and recompile.
2. Delete `extracommands.conf` from your server's config directory.
3. No database rollback is required.
