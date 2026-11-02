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

## Ranged weapons

Any `UOSFWeaponData` can carry a `FOSFRangedStats` profile next to its melee moveset. Ammo is a regular inventory item
(`UOSFAmmoData`: damage per type, poise, status, pellets, source tags); loaded rounds live in a per-weapon magazine on
the equipment component (`GetMagazine`, `Reload`, `CycleAmmo`, `FillMagazine`). `UOSFAction_Shoot` traces from the
shooter's chest towards the lock-on / AI target or the view (aim-assist cone), ignores trigger volumes, and resolves hits
through the combat component; an empty magazine redirects to `UOSFAction_Reload`. `OnShotFired` reports every shot.

## Clip animation (no anim blueprint)

`UOSFClipAnimInstance` evaluates `FOSFAnimClip`s (sequence + start / end / rate / loop) in a custom anim instance proxy:
idle / walk / run blended by ground speed, plus one action clip on top. Actions play `Clip` (or the attack's `Clip`) and
stretch their procedural timing to the clip (or retime the clip with `bFitClipToTiming`), so windows stay data driven.
The root bone is locked to the reference pose: clips are in place and imported root tracks may carry axis conversions.
When a clip carries the pelvis on the root (IK-retargeted clips: root ~1 m up), the bones under the root keep their
component-space pose instead, so retargeted clips neither sink nor lose a death fall's tip-over.
Walk / run cycles play at the rate that matches the capsule speed: `MeasureStrideSpeed` reads a clip's ground speed from
its root travel (root-motion clips) or from the planted-foot speed relative to the hips (in-place clips), times the mesh
scale (`GetLocomotionSpeeds`; `WalkSpeed` / `RunSpeed` are only the fallback). A run that is the walk clip again is dropped.
`AOSFCharacterBase::SetupSkeletalMesh` scales an imported mesh to a height, grounds it and applies a yaw offset.
`DeathClip` plays on death and holds its last frame (`PlayActionClip(..., bHoldAtEnd)`); without one the body tips over
around the feet. A weapon's `StanceIdleClip` / `StanceWalkClip` / `StanceRunClip` replace the locomotion while it is in the
active right hand (`RefreshLocomotionClips`, on equipment changes); `bSheathesOffHand` hides the off-hand weapon for
two-handed guns and draws it for the parry (`UOSFEquipmentComponent::SetOffHandDrawn`).
`bMeshTransformInBoneSpace` marks a MeshTransform authored in the attach bone's own space (e.g. a grip baked by a content
tool against the holder's skeleton): it is scaled with the holder mesh and skips `bAlignWeaponsToOwner`.

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

## Enemy group pacing

`UOSFAICombatComponent::MaxSimultaneousAttackers` (default 2) caps how many regular enemies swing at the same target at
once; the rest keep circling at `PreferredDistance` and retry a moment later (`AOSFAIController::CountOtherAttackers`).
Bosses ignore it.

## Perilous attacks

An attack with `bPerilous` gives its owner `OSF.State.Perilous` during the windup. A hit whose `SourceTags` match the
attack's `CounterTags` cancels it, deals `CounterPoiseDamage` (ignoring hyper armor) and staggers or breaks the stance
(`FOSFHitInfo::bCountered`). `NegatedFraction` tells the attacker how much the target's negation ate (ricochets).

## Quests

`UOSFQuestData` lists objectives (kill by `AOSFEnemyCharacter::KillKey`, progress flag, progress tag, item count) and
rewards. `UOSFQuestComponent` (on the player) stores everything in the progress component: `Quest.<Id>.Active/Done`
flags, `Kill.<Key>` counters and per-quest baselines, so quests are saved and loaded with the rest of the progression.

## Progression and saving

`UOSFProgressComponent` stores gameplay tags, name flags and counters. Interactables persist through
flags (`Chest.<SaveId>`, `Door.<SaveId>`, `Rest.<SaveId>`, `Boss.<SaveId>`…), and vendors track stock with counters.
`UOSFSaveSubsystem` captures the player's stats, items, equipment, progress, respawn point, bloodstain and
time of day into numbered slots, and autosaves when you rest.

## UI

`AOSFPlayerController` owns the UI state machine (menu stack, dialogue, notifications) and exposes it through
`GetMenuEntries / MenuNavigate / MenuConfirm / MenuBack / OnMenuChanged`. `AOSFHUD` draws it on the canvas.
Each `AOSFHUD` section can be switched off (`bDrawStatBars`, `bDrawEquipment`, `bDrawBossBar`, `bDrawPrompt`, ...,
`ExternalMenus`, `ExternalNotifications`) so another layer, such as UMG or an embedded web HUD, can draw it while input stays
with the controller.
`bOpenTitleOnStart` opens a title menu (New Game / Continue / Quit) and adds "Quit to Title" to the game menu; menus in
`PausingMenus` (and the title) pause the world, and menu input keeps working while paused. `IsUsingGamepad()` /
`OnInputDeviceChanged` follow the last meaningful input so prompts and key hints can switch between keyboard and pad glyphs
(`ControlsHintGamepad`, the canvas prompt and menu keys already do). The key hints stay up for `ControlsHintSeconds` of
play the player can actually see (`CanShowControlsHint`, which a game turns off during its title, intro or cutscenes).

## Feedback: sound and effects

Hook points carry their own assets, nothing is hard-wired:
- characters: `HitSound` / `GuardSound` (where the body is struck), `HurtVoice` (with a cooldown), `DeathSound`, `DodgeSound`;
- weapons: `SwingSound`, `ImpactSound`, ranged `FireSound` / `ReloadSound`; items: `UseSound`;
- attacks (`FOSFAttackDefinition`): `StartSound`, `StartEffect` (on the attacker) and `ImpactEffect` (where the hit lands), each
  an `FOSFEffect` (Niagara or Cascade system, tint, scale, offset, attach) spawned through `UOSFCombatLibrary::SpawnEffect`;
- dialogue lines: `FOSFDialogEntry::LineVoices` (same index as `Lines`); menus: `MenuMoveSound` / `MenuConfirmSound` /
  `MenuBackSound` (UI sounds, audible while paused).

Weapons can use a skeletal mesh (`WeaponSkeletalMesh`, e.g. swords with tassels) instead of `StaticMesh`.
To build your own UMG UI, bind to the same API.

## Extension points

- Blueprint subclasses of every actor and component, and data assets for all content
- `UOSFStatComponent::GetLevelUpCost` (BlueprintNativeEvent) for custom level curves
- `AOSFGameMode::ResetWorld / HandlePlayerDeath / RespawnPlayer` (virtual)
- `UOSFInputConfig` to supply your own Enhanced Input assets
- `AOSFEnemyCharacter::BehaviorTree` to replace the built-in AI state machine
