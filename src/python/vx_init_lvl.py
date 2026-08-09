# vx: world 6 level init driven by adventure_info.csv (same data model as the external adventure.py)
# stdlib only so it runs in the embedded python; the alias table below is hardcoded from plants_zombies_info.csv
# Game-facing entry: generate(level) -> list of pot dicts, consumed by VX::GetScaryPotLineup.
import csv
import random
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Optional

# vx: the game resolves its data dir from the working directory (same as properties/),
# so the CSV lives at <cwd>/Properties/adventure_info.csv
_CSV_DIR = Path(getattr(sys, "vx_data_dir", None) or (Path.cwd() / "Properties"))
_ADVENTURE_CSV = _CSV_DIR / "adventure_info.csv"


@dataclass
class InfoInput:
    code1: int
    code2: int
    special: bool
    scene_id: int
    scene_design: str
    scene_rule_code: int
    scene_rule: str
    vase_design: str
    vase_rule_code: int
    vase_rule: str
    slot_rule: str
    beizhu: str
    key: str


_ALIAS2CODE = {
	"pea": 0, "sun": 1, "che": 2, "nut": 3, "min": 4, "sno": 5, "cho": 6, "rep": 7, "puf": 8,
	"ssh": 9, "fum": 10, "gra": 11, "hyp": 12, "sca": 13, "ice": 14, "doo": 15, "lil": 16,
	"squ": 17, "thr": 18, "kel": 19, "jal": 20, "spi": 21, "tor": 22, "tll": 23, "sea": 24,
	"lan": 25, "cac": 26, "blo": 27, "spl": 28, "sta": 29, "pum": 30, "mag": 31, "cab": 32,
	"pot": 33, "ker": 34, "cof": 35, "gar": 36, "umb": 37, "mar": 38, "mel": 39, "gat": 40,
	"twi": 41, "glo": 42, "cat": 43, "win": 44, "gol": 45, "spr": 46, "pao": 47, "exn": 49,
	"gin": 50, "rre": 52, "zom": 100, "flg": 101, "con": 102, "pol": 103, "bkt": 104,
	"pap": 105, "dor": 106, "ftb": 107, "dan": 108, "dab": 109, "duk": 110, "snk": 111,
	"zbn": 112, "zbt": 113, "dol": 114, "jac": 115, "bal": 116, "dig": 117, "pog": 118,
	"yet": 119, "bun": 120, "lad": 121, "ctp": 122, "ggt": 123, "imp": 124, "bos": 125,
	"pez": 126, "nuz": 127, "jaz": 128, "gaz": 129, "sqz": 130, "taz": 131, "gig": 132,
	"ept": -1,
}


def get_input(level) -> InfoInput:
    level = str(level)
    with open(_ADVENTURE_CSV, "r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames:
            reader.fieldnames = [h.strip() for h in reader.fieldnames]  # vx: header cells are space-padded
        for row in reader:
            key = row.get("key")
            if key is not None and key.strip() == level:
                d = {k: v.strip() for k, v in row.items()}  # vx: CSV cells are space-padded
                d["code1"] = int(d["code1"])
                d["code2"] = int(d["code2"])
                d["special"] = d["special"] == "1"
                d["scene_id"] = int(d["scene_id"])
                d["scene_rule_code"] = int(d["scene_rule_code"]) if d["scene_rule_code"] else 0
                d["vase_rule_code"] = int(d["vase_rule_code"]) if d["vase_rule_code"] else 0
                return InfoInput(**d)
    raise ValueError("Level %s not found in adventure_info.csv" % level)


def parse_scene_rules(info: InfoInput):
    # -> (scene_array, scene_pools); mirrors adventure.py semantics
    if info.scene_rule_code != 2:
        return None, None
    rules = {}
    pools = {}
    for part in info.scene_rule.split(";"):
        if len(part) < 2 or part[1] != ":":
            continue
        key = part[0]
        if "*" in part[2:]:
            items = []
            for item in part[2:].split("+"):
                if "*" in item:
                    name, count = item.split("*")
                    items += [name] * int(count)
                else:
                    items.append(item)
            pools[key] = items
            rules[key] = [key]
        else:
            values = part[2:].split("&")
            rules[key] = values
            pools[key] = values
    array = []
    for design_row in info.scene_design.split("/"):
        array.append([rules.get(cell, []) for cell in design_row])
    return array, pools


def parse_vase_rules(info: InfoInput) -> Optional[Dict[str, List[List[str]]]]:
    # -> {category: [[alias, style], ...]}; mirrors adventure.py semantics
    code = info.vase_rule_code
    if code == 1:
        p = []
        for item in info.vase_rule.split("+"):
            if len(item) < 5 or item[3] != "*":
                continue
            p += [[item[:3], "q"]] * int(item[4:])
        return {"1": p}
    if code == 2:
        result = {}
        for part in info.vase_rule.split(";"):
            if len(part) < 5 or part[1] != "@" or part[3] != ":":
                continue
            category = part[0]
            content_type = part[2]
            p = []
            for item in part[4:].split("+"):
                if len(item) < 5 or item[3] != "*":
                    continue
                p += [[item[:3], content_type]] * int(item[4:])
            result[category] = p
        return result
    if code == 3:
        result = {}
        for part in info.vase_rule.split(";"):
            if len(part) < 3 or part[1] != ":":
                continue
            category = part[0]
            p = []
            for item in part[2:].split("+"):
                if len(item) < 6 or item[3] != "*" or item[-2] != "@":
                    continue
                p += [[item[:3], item[-1]]] * int(item[4:-2])
            result[category] = p
        return result
    return None


def generate(level, seed=0) -> list:
    """Game-facing entry: pot lineup for VX::GetScaryPotLineup.
    Each dict: {"row", "col", "type": "seed"|"zombie"|"sun"|"empty", "seed"/"zombie": int code}.
    seed deterministically shuffles each category's content pool, so layouts vary by seed."""
    print("generate!!!")
    info = get_input(level)
    pools = parse_vase_rules(info)
    if not pools:
        return []
    rng = random.Random(seed)
    for pool in pools.values():
        rng.shuffle(pool)
    result = []
    # vase_design is a flat string: 9 digits + "/" per row, so cell (row, col) is at row*10+col
    for row in range(5):
        for col in range(9):
            idx = row * 10 + col
            if idx >= len(info.vase_design):
                continue
            category = info.vase_design[idx]
            if category == "0" or category not in pools or not pools[category]:
                continue
            alias, _style = pools[category].pop(0)  # sequential assignment
            code = _ALIAS2CODE.get(alias, -2)
            if code == -1:  # ept = empty pot
                result.append({"row": row, "col": col, "type": "empty", "count": 1})
            elif code < 0:
                continue  # unknown alias: skip
            elif code < 100:
                result.append({"row": row, "col": col, "type": "seed", "seed": code, "count": 1})
            else:
                result.append({"row": row, "col": col, "type": "zombie", "zombie": code - 100, "count": 1})
    print(result)
    return result


def get_scene(level, seed=0) -> list:
    """Pre-placed plants/zombies: [{"row","col","type":"plant"|"zombie","seed"/"zombie": int}].
    Levels without a CSV row return an empty list."""
    try:
        info = get_input(level)
    except ValueError:
        return []  # vx: unconfigured levels have no scene
    array, pools = parse_scene_rules(info)
    if not array:
        return []
    scene_pools = {k: v[:] for k, v in pools.items()} if pools else {}
    result = []
    for row, design_row in enumerate(array):
        for col, names in enumerate(design_row):
            for name in names:
                if scene_pools and name in scene_pools and scene_pools[name]:
                    name = scene_pools[name].pop(0)
                code = _ALIAS2CODE.get(name, -2)
                if code < 0:
                    continue
                if code < 100:
                    result.append({"row": row, "col": col, "type": "plant", "seed": code})
                else:
                    result.append({"row": row, "col": col, "type": "zombie", "zombie": code - 100})
    return result


def get_slot(level, seed=0) -> dict:
    """Starting sun and seed bank: {"sun": int, "slots": [SeedType int, ...]}. Empty dict if unconfigured."""
    try:
        info = get_input(level)
    except ValueError:
        return {}  # vx: unconfigured levels have no slot setup
    if not info.slot_rule:
        return {}
    if "$" in info.slot_rule:
        sun_str, slots_str = info.slot_rule.split("$", 1)
    else:
        sun_str, slots_str = "", info.slot_rule
    sun = int(sun_str) if sun_str else 0
    slots = []
    for alias in slots_str.split("+"):
        alias = alias.strip()
        if not alias:
            continue
        code = _ALIAS2CODE.get(alias, -1)
        if 0 <= code < 100:  # only plant cards belong in the seed bank
            slots.append(code)
    return {"sun": sun, "slots": slots}
