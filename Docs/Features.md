# Feature matrix

Feature list of the commercial Soulslike Framework (from its forum post) compared with OSF 0.1.

| Feature | OSF 0.1 | Notes |
|---|---|---|
| Combat system | ✅ | C++ action system, procedural or montage driven |
| Weapon-specific animsets | 🟡 | `UOSFWeaponData::AnimLayerClass` is linked on equip; no animations bundled |
| Weapon stancing / two-handing | ✅ | two-handing: 1.5× strength, socket swap |
| Weapon abilities (skills) | ✅ | `SkillAttack` or a custom `SkillActionClass`; parry on shields |
| Guarding and perfect guarding | ✅ | |
| Poise / stance break | ✅ | stagger for players, riposte opening for enemies |
| Finishers (executions and backstabs) | ✅ | |
| Input buffer | ✅ | |
| AI combat and behavior, AI abilities | ✅ | built-in state machine; behavior tree optional |
| Advanced boss AI | ✅ | phases, phase buffs, boss bar, fog gate, music |
| Inventory, crafting, equipment | ✅ | |
| Stats and attributes, status effects, buffs, actions | ✅ | |
| Slot-based saving and loading | ✅ | |
| Tag-based progression | ✅ | tags, flags and counters |
| Level up | ✅ | |
| Resting points | ✅ | |
| NPC dialogue driven by progress | ✅ | |
| Vendors | ✅ | stock, buy and sell |
| Storage | ✅ | |
| Doors, containers, ladders | ✅ | plus pickups, bloodstains, fog gates |
| Radar / compass | ✅ | `UOSFCompassMarkerComponent` |
| Time passing / time of day | ✅ | |
| Full gamepad support | ✅ | Enhanced Input built at runtime; all menus navigable |
| Modular widgets | 🟡 | canvas HUD + menus; UI logic exposed for UMG; no UMG widgets bundled |
| In-editor utility tools | ❌ | roadmap (item / enemy creators) |
| Animations and art | ❌ | none bundled (license); procedural presentation instead |
| Network replication | ❌ | single player |

### Beyond the reference feature list (OSF 0.2)

| Feature | Status | Notes |
|---|---|---|
| Ranged weapons (hitscan) | ✅ | `FOSFRangedStats` on any weapon, `UOSFAmmoData` items, magazines per weapon, reload / ammo cycling, pellets, spread, aim assist, tracers; `Action.Shoot` / `Action.Reload` |
| Perilous attacks and counters | ✅ | `bPerilous` attacks give `State.Perilous` in the windup; a hit with a matching source tag (e.g. `Source.Projectile`) cancels the attack and breaks the stance |
| Hit sources and negation feedback | ✅ | `FOSFDamageSpec::SourceTags`, `FOSFHitInfo::NegatedFraction` (ricochet / "no effect" feedback), `bCountered` |
| Clip animation without anim blueprints | ✅ | `UOSFClipAnimInstance`: idle / walk / run by speed + trimmed action clips (`FOSFAnimClip` = sequence, start, end, rate, loop); root locked to the reference pose for in-place clips (glTF imports) |
| Imported skeletal characters | ✅ | `AOSFCharacterBase::SetupSkeletalMesh` (height, feet, yaw), per-skeleton hand sockets, weapons aligned to the owner for unknown bone axes, static-mesh bodies for unrigged models |
| Quests | ✅ | `UOSFQuestData` (kill / flag / progress / item objectives, rewards) tracked through progress flags and counters, so quests save with the game; HUD tracker |
| Autopilot | ✅ | `UOSFAutoPilotComponent`: scripted steps (MoveTo / Interact / ClearArea / Wait / Custom) + a combat loop through the player's own input paths; games subclass it for their rules (ammo, meters, cutscenes). `OSFAutoPilot start` |
| Runtime translations | ✅ | `UOSFLocalization::LoadTranslationTable` registers a JSON table as polyglot text (template: `Tools/Localization/extract_osf_text.py`); the HUD font falls back to CJK glyphs |

## Roadmap

- UMG widget set
- Editor utilities: item creator, enemy wizard, data validation
- Spells and catalysts, physical projectiles (hitscan guns are in), AoE notifies
- Network replication
- Example Blueprint project using the UE5 mannequin
