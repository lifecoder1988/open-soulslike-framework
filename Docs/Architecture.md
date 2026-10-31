# Architecture

OSF is a single runtime module (`OpenSoulslikeFramework`) plus a test module. Every system is an
`UActorComponent` keyed by **gameplay tags**, configured through **data assets**, and exposed to Blueprint.

```
AOSFCharacterBase (ACharacter, IGameplayTagAssetInterface)
├── UOSFStatComponent          stats (HP/FP/Stamina/Poise/EquipLoad), attributes, level, modifiers
├── UOSFActionComponent        action manager, input buffer, state tags
├── UOSFCombatComponent        hit resolution, reactions, finishers, death
├── UOSFStatusEffectComponent  buildup effects
├── UOSFBuffComponent          timed modifiers
├── UOSFInventoryComponent     items, storage, currency, flasks, crafting
├── UOSFEquipmentComponent     weapon/armor/talisman/quick slots, weapon actors
└── UOSFWeaponTraceComponent   blade / sphere sweeps
AOSFPlayerCharacter  + UOSFLockOnComponent, UOSFInteractionComponent, UOSFProgressComponent, camera, input
AOSFEnemyCharacter   + UOSFAICombatComponent, UOSFBossComponent, AOSFAIController
```

## Actions

`UOSFAction` is a UObject instanced per character. The action component runs one action at a time.

- **Windows**: `Active` (damage), `Invincible`, `HyperArmor`, `Cancel` (combo / buffer) and `Parry`.
  In *procedural mode* they follow `FOSFActionTiming` (windup, active, recovery). In *montage mode*
  they are opened by the OSF anim notify states. The game logic is identical in both modes.
- **Input buffer**: an input pressed during an action is kept until that action's cancel window opens,
  then fires. `CancelledByActions` lets dodges cancel attacks at any time.
- **Redirects**: `GetRedirect()` lets an action hand over to another one. A light attack becomes a
  riposte or backstab, and the weapon skill becomes a parry when holding a parrying shield.
- Built-ins: light, heavy, weapon skill and AI ability attacks, dodge (roll / backstep), guard, parry, use item, execute.
- Custom actions: subclass `UOSFAction` (C++ or Blueprint), add it to `DefaultActions`, or set it as a weapon's `SkillActionClass`.

## Hit resolution (`UOSFCombatComponent::ReceiveDamage`)

1. Ignored if dead, invincible, being executed, or on the same team.
2. **Parry**: the victim's parry window is open, the hit is parryable, and the victim faces the attacker. The attacker becomes open to a riposte.
3. **Guard**: the victim faces the attacker. A perfect guard (first frames of the guard) blocks everything.
   Otherwise absorption comes from the guarding weapon, stamina damage is reduced by guard boost,
   and the guard breaks when stamina runs out.
4. **Hit**: negation from armor, natural negation and modifiers, combined multiplicatively; buff
   multipliers; status buildup; poise damage. With poise at 0 you get a stagger (players) or a
   stance break that opens a riposte (enemies, `bPoiseBreakOpensRiposte`).

## Data

| Asset | Purpose |
|---|---|
| `UOSFItemData` | base item: id, name, icon, category, stack, price, use effects, flask flag |
| `UOSFWeaponData` | damage per type, scaling, requirements, poise, crit, status buildup, guard stats, moveset (`FOSFAttackDefinition`), skill, anim layer |
| `UOSFArmorData` / `UOSFTalismanData` | negation, poise, modifiers, modular mesh / passive buff |
| `UOSFBuffData` / `UOSFStatusEffectData` | timed modifiers / buildup effects |
| `UOSFCraftingRecipe`, `UOSFDialogData`, `UOSFVendorData` | crafting, progress-driven dialogue, shops |

Items are saved by `ItemId`. `UOSFItemRegistry` resolves ids for data assets (through the asset registry) and for items created at runtime.

## Progression and saving

`UOSFProgressComponent` stores gameplay tags, name flags and counters. Interactables persist through
flags (`Chest.<SaveId>`, `Door.<SaveId>`, `Rest.<SaveId>`, `Boss.<SaveId>`…), and vendors track stock with counters.
`UOSFSaveSubsystem` captures the player's stats, items, equipment, progress, respawn point, bloodstain and
time of day into numbered slots, and autosaves when you rest.

## UI

`AOSFPlayerController` owns the UI state machine (menu stack, dialogue, notifications) and exposes it through
`GetMenuEntries / MenuNavigate / MenuConfirm / MenuBack / OnMenuChanged`. `AOSFHUD` draws it on the canvas.
To build your own UMG UI, bind to the same API.

## Extension points

- Blueprint subclasses of every actor and component, and data assets for all content
- `UOSFStatComponent::GetLevelUpCost` (BlueprintNativeEvent) for custom level curves
- `AOSFGameMode::ResetWorld / HandlePlayerDeath / RespawnPlayer` (virtual)
- `UOSFInputConfig` to supply your own Enhanced Input assets
- `AOSFEnemyCharacter::BehaviorTree` to replace the built-in AI state machine
