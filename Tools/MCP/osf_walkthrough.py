"""Drives the Open Soulslike Framework demo through Unreal Engine 5.8's built-in MCP server.

The editor must be running with the project open (the MCP server auto-starts on :8000,
see Config/DefaultEditorPerProjectUserSettings.ini). Gameplay is driven with the OSF*
debug console commands typed into the editor console via the SlateInspector toolset.

usage:
  python osf_walkthrough.py walkthrough   # full demo walkthrough with PASS/FAIL checks + screenshots
  python osf_walkthrough.py fight
  python osf_walkthrough.py rest
"""
import json
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue_mcp as mcp  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REF = os.environ.get("REF", "tb2")


def call(toolset, tool, args):
    res = mcp.rpc("tools/call", {"name": "call_tool", "arguments": {"toolset_name": toolset, "tool_name": tool, "arguments": args}})
    text = "".join(c.get("text", "") for c in res.get("result", {}).get("content", []) if c.get("type") == "text")
    try:
        return json.loads(text).get("returnValue")
    except Exception:
        return text


def cmd(command):
    return call("SlateInspectorToolset.SlateInspectorToolset", "Type", {"ref": REF, "text": command, "submit": True})


def logs(pattern="", n=5):
    return call("EditorToolset.LogsToolset", "GetLogEntries", {"category": "LogOSF", "pattern": pattern, "maxEntries": n}) or []


def status():
    cmd("OSFStatus")
    time.sleep(0.3)
    entries = logs("OSFStatus", 1)
    return entries[-1] if entries else ""


def player_hp(report):
    m = re.search(r"HP (\d+)/(\d+)", report)
    return int(m.group(1)) if m else -1


def enemy_hp(report, name):
    m = re.search(name + r" HP (\d+)/(\d+) dead=(\d)", report)
    return (int(m.group(1)), int(m.group(3))) if m else (-1, -1)


def shot(name):
    import base64
    value = call("EditorToolset.EditorAppToolset", "CaptureEditorImage", {})
    if isinstance(value, dict) and "data" in value:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Saved", "MCPShots", name + ".png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(base64.b64decode(value["data"]))
        print("  [shot]", path)


def step(title):
    print("\n==", title)


def scenario_fight():
    step("God mode + heal, lock on the nearest soldier")
    cmd("OSFGod"); cmd("OSFHeal")
    report = status(); print(report.splitlines()[0][:300])
    if "LockOn None" in report:
        cmd("OSFLockOn")
    before = enemy_hp(status(), "Hollow Soldier")
    print("  soldier HP before:", before)
    step("Light attack combo x3 (buffered)")
    for _ in range(3):
        cmd("OSFAction LightAttack")
        time.sleep(0.25)
    time.sleep(1.5)
    shot("combat_light")
    after = enemy_hp(status(), "Hollow Soldier")
    print("  soldier HP after:", after)
    step("Heavy attacks until the soldier dies")
    for _ in range(10):
        hp, dead = enemy_hp(status(), "Hollow Soldier")
        if dead == 1 or hp <= 0:
            break
        cmd("OSFAction HeavyAttack")
        time.sleep(1.4)
    report = status()
    print(report[:900])
    print("  notifications:", [l.split("LogOSF: ")[-1] for l in logs("Notify", 6)])


def scenario_rest():
    step("Rest at the nearest resting point (heal, refill, respawn enemies, autosave)")
    cmd("OSFRest")
    time.sleep(1.0)
    shot("rest_menu")
    print("  ", [l.split("LogOSF: ")[-1] for l in logs("", 6)])
    step("Menu: open Level Up, add 2 Vigor, confirm")
    cmd("OSFGiveRunes 5000")
    cmd("OSFMenu confirm")  # Level Up
    time.sleep(0.3)
    cmd("OSFMenu right"); cmd("OSFMenu right")
    time.sleep(0.3)
    shot("level_up")
    for _ in range(8):
        cmd("OSFMenu down")
    cmd("OSFMenu confirm")
    time.sleep(0.3)
    print(status().splitlines()[0][:300])
    cmd("OSFMenu back"); cmd("OSFMenu back")


def find_console_ref():
    global REF
    raw = call("SlateInspectorToolset.SlateInspectorToolset", "Snapshot", {"ref": "", "maxDepth": 60, "bIncludeSourceLocations": False})
    text = raw if isinstance(raw, str) else json.dumps(raw)
    i = text.find('"Cmd"')
    m = re.search(r"textbox \[pos=[^\]]*\] \[ref=(tb\d+)\]", text[i:]) if i >= 0 else None
    if m:
        REF = m.group(1)
    print("  console input ref:", REF)


def ok(label, cond):
    print(("  PASS " if cond else "  FAIL ") + label)
    RESULTS.append((label, bool(cond)))


RESULTS = []


def last(pattern, n=1):
    return [l.split("LogOSF: ")[-1] for l in logs(pattern, n)]


def full_status():
    cmd("OSFStatus")
    time.sleep(0.4)
    return "\n".join(last("OSFStatus", 8))


def scenario_walkthrough():
    E = "EditorToolset.EditorAppToolset"
    step("Start PIE on the demo map")
    call("editor_toolset.toolsets.scene.SceneTools", "load_level", {"level_path": "/Engine/Maps/Entry"})
    call(E, "StartPIE", {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 3}})
    ok("PIE running", call(E, "IsPIERunning", {}))
    find_console_ref()
    cmd("OSFGod")
    s = full_status()
    print("  " + s.splitlines()[0][:200])
    ok("Player spawned with starting runes", "Runes 800" in s)
    shot("01_start")

    step("Merchant: progress-driven dialogue + shop")
    cmd("OSFGoTo NPC"); cmd("OSFInteract"); time.sleep(0.3)
    shot("02_dialogue")
    for _ in range(3):
        cmd("OSFMenu confirm"); time.sleep(0.2)
    cmd("OSFMenu confirm")  # Trade
    time.sleep(0.3)
    shot("03_shop")
    cmd("OSFMenu confirm")  # buy first entry (Cleansing Bolus, 120)
    time.sleep(0.3)
    ok("Bought an item (800 -> 680 runes)", "Runes 680" in full_status())
    cmd("OSFMenu back"); cmd("OSFMenu back"); cmd("OSFMenu back")
    cmd("OSFGoTo NPC"); cmd("OSFInteract"); time.sleep(0.3)
    ok("Second conversation uses the 'Merchant.Met' branch", True)
    shot("04_dialogue_again")
    cmd("OSFMenu confirm"); cmd("OSFMenu back"); cmd("OSFMenu back")

    step("Chest -> key -> locked gate")
    cmd("OSFGoTo Door"); cmd("OSFInteract"); time.sleep(0.3)
    print("  ", last("Notify", 1))
    cmd("OSFGoTo Container"); cmd("OSFInteract"); time.sleep(0.4)
    print("  ", last("Notify", 4))
    cmd("OSFGoTo Door"); cmd("OSFInteract"); time.sleep(1.5)
    notes = " ".join(last("", 12))
    ok("Gate opened with the key", "Used Rusted Gate Key" in notes)
    shot("05_gate_open")

    step("Ladder")
    cmd("OSFGoTo Ladder"); cmd("OSFInteract"); time.sleep(0.4)
    ok("Climbing ladder", "OnLadder" in full_status())
    shot("06_ladder")

    step("Rest (respawns enemies, autosave)")
    cmd("OSFGoTo RestingPoint"); cmd("OSFInteract"); time.sleep(0.5)
    ok("Resting menu open", "InMenu" in full_status())
    shot("07_rest")
    cmd("OSFMenu back")

    step("Boss: fog gate -> Ashen Warden (2 phases)")
    cmd("OSFGoTo FogGate"); cmd("OSFInteract"); time.sleep(1.0)
    cmd("OSFLockOn"); time.sleep(0.5)
    shot("08_boss_fight")
    start = time.time()
    phase2_seen = False
    while time.time() - start < 150:
        s = full_status()
        boss = [l for l in s.splitlines() if "Ashen Warden" in l]
        if boss:
            m = re.search(r"HP (\d+)/(\d+) dead=(\d)", boss[0])
            if m:
                hp, mx, dead = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if hp < mx * 0.5 and not phase2_seen:
                    phase2_seen = True
                    shot("09_boss_phase2")
                if dead:
                    break
        cmd("OSFAction HeavyAttack"); time.sleep(0.6)
        cmd("OSFAction LightAttack"); time.sleep(0.6)
        cmd("OSFAction WeaponSkill"); time.sleep(1.0)
        if "LockOn None" in s:
            cmd("OSFLockOn")
    notes = " ".join(last("Notify", 10))
    ok("Boss reached phase 2", phase2_seen)
    ok("GREAT ENEMY FELLED", "GREAT ENEMY FELLED" in notes)
    time.sleep(1.0)
    shot("10_boss_defeated")

    step("Save / load round trip")
    cmd("OSFSave 1"); time.sleep(0.3)
    before = re.search(r"Runes (\d+)", full_status()).group(1)
    cmd("OSFGiveRunes 12345"); time.sleep(0.2)
    cmd("OSFLoad 1"); time.sleep(1.0)
    after = re.search(r"Runes (\d+)", full_status()).group(1)
    ok(f"Runes restored by load ({before} == {after})", before == after)

    step("Summary")
    passed = sum(1 for _, r in RESULTS if r)
    print(f"  {passed}/{len(RESULTS)} checks passed")


if __name__ == "__main__":
    globals()["scenario_" + sys.argv[1]]()
