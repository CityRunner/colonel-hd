#!/usr/bin/env python3
"""Checks the hint files in hints/ (spec section 6.7) without the game.

Every hints/pic.NNN.json must satisfy hints/schema.json and the rules the
schema cannot state: the file name matches the picture, ids are unique,
no line is in two chains, a gap joins two different lines, and symmetry
refers to chains of the file, each once, and relations name regions or
chains of the file. For the scene model of version 2 (A4): ids are unique
across the whole file, version 1 files have no objects or repetitions,
primitives have the points their kind needs and corners at their points,
geometric relations name parts or primitives of their own hypothesis,
organic objects get no idealisation, repetitions are objects of one type or
parts of one role, and a decision names a hypothesis of its object. The
program's reader (sci-vector, parse_hints) applies the same rules; whether
the lines and commands exist in the picture is checked with the game files
by sci-hint-tool check.

The schema is interpreted by a small validator for the keywords it uses,
so the check needs nothing beyond the Python standard library.

Usage: check-hints.py [directory]   (default: hints/ of the repository)
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILE_NAME = re.compile(r"^pic\.(\d{3})\.json$")


def reject_duplicates(pairs):
    names = [name for name, _ in pairs]
    if len(names) != len(set(names)):
        raise ValueError("duplicate member")
    return dict(pairs)


def reject_floats(text):
    raise ValueError(f"only integers are allowed: {text}")


class Validator:
    def __init__(self, schema):
        self.schema = schema
        self.errors = []

    def resolve(self, node):
        while "$ref" in node:
            ref = node["$ref"]
            if not ref.startswith("#/$defs/"):
                raise ValueError(f"unsupported reference {ref}")
            node = self.schema["$defs"][ref[len("#/$defs/"):]]
        return node

    def fail(self, where, what):
        self.errors.append(f"{where}: {what}")

    def check(self, node, value, where):
        node = self.resolve(node)
        if "const" in node and value != node["const"]:
            self.fail(where, f"{node['const']!r} expected")
        if "enum" in node and value not in node["enum"]:
            self.fail(where, f"one of {node['enum']} expected")
        kind = node.get("type")
        checks = {
            "object": lambda v: isinstance(v, dict),
            "array": lambda v: isinstance(v, list),
            "string": lambda v: isinstance(v, str),
            "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
            "boolean": lambda v: isinstance(v, bool),
        }
        if kind is not None and not checks[kind](value):
            self.fail(where, f"{kind} expected")
            return
        if kind == "object":
            self.check_object(node, value, where)
        elif kind == "array":
            self.check_array(node, value, where)
        elif kind == "string":
            if len(value) > node.get("maxLength", len(value)):
                self.fail(where, "string too long")
            if "pattern" in node and not re.search(node["pattern"], value):
                self.fail(where, f"does not match {node['pattern']}")
        elif kind == "integer":
            if value < node.get("minimum", value) or value > node.get("maximum", value):
                self.fail(where, f"{value} out of range")

    def check_object(self, node, value, where):
        properties = node.get("properties", {})
        for name in node.get("required", []):
            if name not in value:
                self.fail(where, f"member {name!r} missing")
        for name, member in value.items():
            if name in properties:
                self.check(properties[name], member, f"{where}.{name}")
            elif node.get("additionalProperties", True) is False:
                self.fail(where, f"unknown member {name!r}")

    def check_array(self, node, value, where):
        if len(value) < node.get("minItems", 0) or len(value) > node.get("maxItems", len(value)):
            self.fail(where, "wrong number of items")
        prefix = node.get("prefixItems", [])
        for i, item in enumerate(value):
            if i < len(prefix):
                self.check(prefix[i], item, f"{where}[{i}]")
            elif "items" in node:
                self.check(node["items"], item, f"{where}[{i}]")


def check_rules(hints, name, number):
    errors = []
    if hints.get("picture") != number:
        errors.append(f"{name}: the file is named for another picture")
    lines = [line for chain in hints.get("chains", []) for line in chain["lines"]]
    errors += [f"{name}: line {n} is in two chains" for n in sorted({n for n in lines if lines.count(n) > 1})]
    for gap in hints.get("gaps", []):
        if gap["from"] == gap["to"]:
            errors.append(f"{name}: gap {gap['id']!r} joins a line to itself")
    chains = {chain["id"] for chain in hints.get("chains", [])}
    for i, symmetry in enumerate(hints.get("symmetry", [])):
        refs = symmetry["chains"]
        if len(refs) != len(set(refs)) or not set(refs) <= chains:
            errors.append(f"{name}: symmetry[{i}] must list chains of the file, each once")
    targets = {item["id"] for key in ("regions", "chains") for item in hints.get(key, [])}
    seen = set()
    for i, relation in enumerate(hints.get("relations", [])):
        key = (relation["kind"], relation["a"], relation["b"])
        if relation["a"] not in targets or relation["b"] not in targets:
            errors.append(f"{name}: relations[{i}] must name regions or chains of the file")
        elif relation["a"] == relation["b"]:
            errors.append(f"{name}: relations[{i}] relates an item to itself")
        elif key in seen:
            errors.append(f"{name}: relations[{i}] is listed twice")
        seen.add(key)
    errors += check_scene(hints, name)
    return errors


ORGANIC = {"foliage", "tree", "rock", "cloud", "water", "terrain", "figure"}
POINTS = {"line": (2, 2), "arc": (3, 3), "ellipse": (3, 512), "spline": (2, 512), "polyline": (2, 512)}
MIN_ITEMS = {"parallel": 2, "perpendicular": 2, "equal-spacing": 3, "symmetry": 2, "tangent": 2, "aligned": 2}
PAIRS = {"perpendicular", "tangent"}
WITH_AXIS = {"symmetry", "aligned"}


def valid_date(text):
    year, month, day = (int(part) for part in text.split("-"))
    leap = year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)
    days = [31, 29 if leap else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    return 1 <= month <= 12 and 1 <= day <= days[month - 1]


def hypotheses(obj):
    """The main hypothesis (named by the object's id), then the alternatives."""
    yield obj
    yield from obj.get("alternatives", [])


def check_hypothesis(hypothesis, where):
    errors = []
    organic = hypothesis["type"] in ORGANIC
    parts = {part["id"] for part in hypothesis["parts"]}
    primitives = {p["id"]: p for part in hypothesis["parts"] for p in part["primitives"]}
    for primitive in primitives.values():
        here = f"{where}: primitive {primitive['id']!r}"
        low, high = POINTS[primitive["kind"]]
        if not low <= len(primitive["points"]) <= high:
            errors.append(f"{here}: a {primitive['kind']} has {low} to {high} points")
        if len(set(primitive["commands"])) != len(primitive["commands"]):
            errors.append(f"{here}: a command is listed twice")
        corners = [corner["point"] for corner in primitive.get("corners", [])]
        if len(set(corners)) != len(corners) or any(c >= len(primitive["points"]) for c in corners):
            errors.append(f"{here}: corners are at points of the primitive, each once")
        if organic and primitive["kind"] not in ("spline", "polyline"):
            errors.append(f"{here}: an organic object has only splines and polylines")
    if organic and hypothesis.get("relations"):
        errors.append(f"{where}: an organic object has no geometric relations")
    for i, relation in enumerate(hypothesis.get("relations", [])):
        here = f"{where}: relations[{i}]"
        kind, items = relation["kind"], relation["items"]
        if len(set(items)) != len(items) or not all(item in parts or item in primitives for item in items):
            errors.append(f"{here} must name parts or primitives of its hypothesis, each once")
            continue
        if len(items) < MIN_ITEMS[kind] or (kind in PAIRS and len(items) != 2):
            errors.append(f"{here}: wrong number of items for {kind}")
        if (kind in WITH_AXIS) != ("axis" in relation):
            errors.append(f"{here}: symmetry and aligned have an axis, the other kinds none")
        if kind == "tangent":
            kinds = sorted(primitives[item]["kind"] if item in primitives else "part" for item in items)
            if kinds != ["arc", "line"]:
                errors.append(f"{here}: a tangent joins an arc and a line primitive")
    return errors


def check_scene(hints, name):
    errors = []
    objects = hints.get("objects", [])
    if hints["version"] == 1 and ("objects" in hints or "repetitions" in hints):
        errors.append(f"{name}: objects and repetitions need version 2")
    ids = [item["id"] for key in ("regions", "chains", "gaps", "repetitions") for item in hints.get(key, [])]
    part_of = {}  # Part id: (role, whether its hypothesis is organic).
    object_types = {}
    for obj in objects:
        object_types[obj["id"]] = obj["type"]
        for hypothesis in hypotheses(obj):
            ids.append(hypothesis["id"])
            for part in hypothesis["parts"]:
                ids.append(part["id"])
                ids += [p["id"] for p in part["primitives"]]
                part_of[part["id"]] = (part["role"], hypothesis["type"] in ORGANIC)
            errors += check_hypothesis(hypothesis, f"{name}: object {obj['id']!r}")
        decision = obj.get("decision")
        if decision:
            if decision["hypothesis"] not in [h["id"] for h in hypotheses(obj)]:
                errors.append(f"{name}: object {obj['id']!r}: the decision names no hypothesis of the object")
            if not valid_date(decision["date"]):
                errors.append(f"{name}: object {obj['id']!r}: the decision's date is not a date")
    errors += [f"{name}: id {i!r} is used twice" for i in sorted({i for i in ids if ids.count(i) > 1})]
    repeated = []
    for repetition in hints.get("repetitions", []):
        here = f"{name}: repetition {repetition['id']!r}"
        items = repetition["items"]
        repeated += items
        if all(item in object_types for item in items):
            kinds = {object_types[item] for item in items}
            organic = bool(kinds & ORGANIC)
        elif all(item in part_of for item in items):
            kinds = {part_of[item][0] for item in items}
            organic = any(part_of[item][1] for item in items)
        else:
            errors.append(f"{here} must name objects, or parts, of the file")
            continue
        if len(kinds) != 1:
            errors.append(f"{here}: the items are of one type or of one role")
        if organic:
            errors.append(f"{here}: organic shapes are not repeated identically")
    errors += [f"{name}: {i!r} is in two repetitions" for i in sorted({i for i in repeated if repeated.count(i) > 1})]
    return errors


def main():
    directory = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "hints"
    schema = json.loads((ROOT / "hints" / "schema.json").read_text())
    errors = []
    files = sorted(p for p in directory.glob("*.json") if p.name != "schema.json")
    for path in files:
        match = FILE_NAME.match(path.name)
        if not match:
            errors.append(f"{path.name}: hint files are named pic.NNN.json")
            continue
        try:
            text = path.read_bytes().decode("ascii")
            hints = json.loads(
                text, object_pairs_hook=reject_duplicates, parse_float=reject_floats, parse_constant=reject_floats
            )
        except (UnicodeDecodeError, ValueError) as error:
            errors.append(f"{path.name}: {error}")
            continue
        validator = Validator(schema)
        validator.check(schema, hints, path.name)
        errors += validator.errors
        if not validator.errors:
            errors += check_rules(hints, path.name, int(match.group(1)))
    for error in errors:
        print(error)
    print(f"{len(files)} hint files, {len(errors)} problems")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
