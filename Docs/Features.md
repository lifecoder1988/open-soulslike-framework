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

## Roadmap

- UMG widget set
- Editor utilities: item creator, enemy wizard, data validation
- Spells and catalysts, projectiles, AoE notifies
- Network replication
- Example Blueprint project using the UE5 mannequin
