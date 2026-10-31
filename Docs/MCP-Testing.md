# Testing with MCP (Model Context Protocol)

Unreal Engine 5.8 ships an experimental **Unreal MCP** server plugin (`ModelContextProtocol`) and a set
of *toolsets*. This project enables both, plus `AllToolsets`, and auto-starts the server:

```ini
; Config/DefaultEditorPerProjectUserSettings.ini
[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
bAutoStartServer=True
ServerPortNumber=8000

[/Script/UnrealEd.EditorPerformanceSettings]
bThrottleCPUWhenNotForeground=False   ; keep PIE at full speed while an agent drives it
```

Once the editor is open, any MCP client can connect to `http://127.0.0.1:8000/mcp`. The included
`.mcp.json` registers it for Claude Code. The server exposes three meta tools: `list_toolsets`,
`describe_toolset` and `call_tool`.

## Toolsets used by this project

| Toolset | Used for |
|---|---|
| `AutomationTestToolset` | `DiscoverTests`, `RunTestsByFilter("OSF")`, `GetTestStatus`, `GetTestResults` |
| `EditorToolset.EditorAppToolset` | `StartPIE`, `StopPIE`, `IsPIERunning`, `CaptureEditorImage` |
| `editor_toolset.toolsets.scene.SceneTools` | `load_level`, `find_actors`, `add_to_scene_from_class` |
| `EditorToolset.LogsToolset` | `GetLogEntries(category="LogOSF")` to read game state |
| `SlateInspectorToolset` | `Snapshot` (find the console box), `Type` (run console commands) |
| `editor_toolset.toolsets.object.ObjectTools` | read and write settings / CDO properties |

Live Coding works through MCP too: type `LiveCoding.Compile` into the console, then poll `LogLiveCoding`.

## OSF debug commands

| Command | Effect |
|---|---|
| `OSFStatus` | logs player and enemy state (one `LogOSF` line each) |
| `OSFAction <LightAttack/HeavyAttack/Dodge/Guard/WeaponSkill/UseItem>` | performs an action (same path as player input) |
| `OSFGoTo <filter>` | teleports in front of an actor (`NPC`, `Container`, `Door`, `Ladder`, `RestingPoint`, `FogGate`…) |
| `OSFInteract` | interacts with the focused interactable |
| `OSFMenu <up/down/left/right/confirm/back/open>` | drives the menus |
| `OSFLockOn`, `OSFGod`, `OSFHeal`, `OSFKillAll`, `OSFRest`, `OSFGiveRunes n`, `OSFGiveItem id n`, `OSFSave n`, `OSFLoad n`, `OSFDebugTraces` | utilities |

## End-to-end walkthrough

```bash
python Tools/MCP/osf_walkthrough.py walkthrough
```

This loads the map, starts PIE, then talks to the merchant (and checks the dialogue branch changes),
buys an item, tries the locked gate, opens the chest for the key, opens the gate, climbs the ladder,
rests, enters the boss fog gate, fights the two-phase boss to the end, and checks save/load.
Screenshots are written to `Saved/MCPShots`.

Example output (UE 5.8, Windows):

```
== Merchant: progress-driven dialogue + shop
  PASS Bought an item (800 -> 680 runes)
== Chest -> key -> locked gate
  PASS Gate opened with the key
== Ladder
  PASS Climbing ladder
== Rest (respawns enemies, autosave)
  PASS Resting menu open
== Boss: fog gate -> Ashen Warden (2 phases)
  PASS Boss reached phase 2
  PASS GREAT ENEMY FELLED
== Save / load round trip
  PASS Runes restored by load (5880 == 5880)
  10/10 checks passed
```

## Automation tests

| Test | Covers |
|---|---|
| `OSF.Stats.AttributesLevelAndModifiers` | stat scaling, level-up cost and currency, modifiers, stamina regen delay |
| `OSF.Combat.HitGuardPerfectGuardIFrames` | hits, friendly fire, perfect guard, guard, guard break, back hits, roll i-frames, death and revive |
| `OSF.Combat.ParryPoiseBreakRiposteBackstab` | parry, riposte execution, stance break, backstab detection |
| `OSF.Combat.WeaponTraceIntegration` | a real attack action hitting through the weapon trace, exact attack-rating math |
| `OSF.Actions.ComboChainAndInputBuffer` | input buffer, combo index, dodge cancel, stamina gating |
| `OSF.Status.BleedPoisonFrostbite` | buildup, procs, DoT, debuffs, resistance, decay |
| `OSF.Items.InventoryEquipmentFlasksCrafting` | flasks, stack overflow to storage, crafting, armor poise and negation |
| `OSF.Save.CaptureAndApplyRoundTrip` | save capture and apply |
| `OSF.Progress.DialogSelectionAndVendor` | progress-driven dialogue, vendor stock, buying and selling |
| `OSF.AI.AbilitySelectionAndBossPhases` | ability ranges and cooldowns, boss phase transitions |

Run them headless with `UnrealEditor-Cmd OSFDemo.uproject -ExecCmds="Automation RunTests OSF;Quit" -unattended -NullRHI -testexit="Automation Test Queue Empty"`,
or through MCP with `AutomationTestToolset.RunTestsByFilter`.
