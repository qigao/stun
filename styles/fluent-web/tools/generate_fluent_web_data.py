#!/usr/bin/env python3
import ast
import json
import math
import re
from collections import OrderedDict
from pathlib import Path

BASE = Path(__file__).resolve().parents[3] / "package" / "lib"


def read_text(*parts):
    return (BASE.joinpath(*parts)).read_text(encoding="utf8")


def parse_js_object(text):
    entries = OrderedDict()
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("//") or line.startswith("/*"):
            continue
        line = line.rstrip(",")
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip().strip("`'\"")
        value = value.strip().rstrip(",}")
        if value.startswith(("'", '"', "`")) and value.endswith(("'", '"', "`")):
            value = value.strip("'\"`")
        entries[key] = value
    return entries


def parse_exported_objects(src):
    objects = {}
    object_pattern = re.compile(r"export const (\w+) = \{\s*(.*?)\s*\};", re.S)
    value_pattern = re.compile(r"export const (\w+) = (`[^`]+`|'[^']+'|\"[^\"]+\");")
    for name, body in object_pattern.findall(src):
        objects[name] = parse_js_object(body)
    for name, value in value_pattern.findall(src):
        objects[name] = value.strip("`'\"")
    return objects


def literal_list(src, name):
    match = re.search(rf"export const {name}\s*=\s*\[(.*?)\];", src, re.S)
    if not match:
        raise RuntimeError(f"Failed to parse list {name}")
    list_text = "[" + match.group(1) + "]"
    return ast.literal_eval(list_text)


def eval_expr(expr, ctx):
    expr = expr.strip()
    if expr.endswith(","):
        expr = expr[:-1]
    if expr.startswith(("'", '"', "`")):
        return expr.strip("'\"`")
    if expr == "white":
        return ctx["white"]
    if expr == "black":
        return ctx["black"]
    if expr == "brand[160]":
        return ctx["brand"]["160"]
    if "[" in expr and "]" in expr:
        prefix, rest = expr.split("[", 1)
        key = rest.split("]", 1)[0]
        key = key.strip("'\"")
        return ctx[prefix][key]
    if expr == "brand":
        raise RuntimeError("Unexpected bare brand reference")
    if expr == "transparent":
        return "transparent"
    return expr


def to_rgba(value):
    if isinstance(value, tuple):
        return value
    value = value.strip()
    if value == "transparent":
        return (0.0, 0.0, 0.0, 0.0)
    if value.startswith("#") and len(value) == 7:
        r = int(value[1:3], 16)
        g = int(value[3:5], 16)
        b = int(value[5:7], 16)
        return (r / 255.0, g / 255.0, b / 255.0, 1.0)
    if value.startswith("rgba"):
        numbers = [float(x.strip()) for x in re.findall(r"[\d.]+", value)]
        r, g, b, a = numbers
        return (r / 255.0, g / 255.0, b / 255.0, a)
    raise RuntimeError(f"Unsupported color value: {value}")


def build_color_tokens(light_src, dark_src, ctx, brand):
    def parse_body(src):
        body_match = re.search(r"\(\s*brand\s*\)\s*=>\s*\(\{\s*(.*?)\s*\}\);", src, re.S)
        if not body_match:
            raise RuntimeError("Failed to parse color token body")
        body = body_match.group(1)
        result = OrderedDict()
        for line in body.splitlines():
            line = line.strip()
            if not line or line.startswith("//"):
                continue
            if ":" not in line:
                continue
            key, expr = line.split(":", 1)
            key = key.strip()
            result[key] = expr.strip()
        return result

    light_map = parse_body(light_src)
    dark_map = parse_body(dark_src)
    if light_map.keys() != dark_map.keys():
        raise RuntimeError("Light and dark token sets differ")

    tokens = OrderedDict()
    for key in light_map.keys():
        light_value = eval_expr(light_map[key], ctx | {"brand": brand})
        dark_value = eval_expr(dark_map[key], ctx | {"brand": brand})
        tokens[key] = (to_rgba(light_value), to_rgba(dark_value))
    return tokens


def build_palette_tokens(ctx, status_names, persona_names, status_mapping, mapped_status_names):
    tokens = OrderedDict()
    for shared in status_names:
        color = shared[0].upper() + shared[1:]
        shades = ctx[shared]
        tokens[f"colorPalette{color}Background1"] = to_rgba(shades["tint60"])
        tokens[f"colorPalette{color}Background2"] = to_rgba(shades["tint40"])
        tokens[f"colorPalette{color}Background3"] = to_rgba(shades["primary"])
        tokens[f"colorPalette{color}Foreground1"] = to_rgba(shades["shade10"])
        tokens[f"colorPalette{color}Foreground2"] = to_rgba(shades["shade30"])
        tokens[f"colorPalette{color}Foreground3"] = to_rgba(shades["primary"])
        tokens[f"colorPalette{color}BorderActive"] = to_rgba(shades["primary"])
        tokens[f"colorPalette{color}Border1"] = to_rgba(shades["tint40"])
        tokens[f"colorPalette{color}Border2"] = to_rgba(shades["primary"])
    tokens["colorPaletteYellowForeground1"] = to_rgba(ctx["yellow"]["shade30"])
    tokens["colorPaletteRedForegroundInverted"] = to_rgba(ctx["red"]["tint20"])
    tokens["colorPaletteGreenForegroundInverted"] = to_rgba(ctx["green"]["tint20"])
    tokens["colorPaletteYellowForegroundInverted"] = to_rgba(ctx["yellow"]["tint40"])

    for shared in persona_names:
        color = shared[0].upper() + shared[1:]
        shades = ctx[shared]
        tokens[f"colorPalette{color}Background2"] = to_rgba(shades["tint40"])
        tokens[f"colorPalette{color}Foreground2"] = to_rgba(shades["shade30"])
        tokens[f"colorPalette{color}BorderActive"] = to_rgba(shades["primary"])

    mapped_status = {name: ctx[name] for name in mapped_status_names}
    for status, shared_name in status_mapping.items():
        color = status[0].upper() + status[1:]
        shades = mapped_status[shared_name]
        tokens[f"colorStatus{color}Background1"] = to_rgba(shades["tint60"])
        tokens[f"colorStatus{color}Background2"] = to_rgba(shades["tint40"])
        tokens[f"colorStatus{color}Background3"] = to_rgba(shades["primary"])
        tokens[f"colorStatus{color}Foreground1"] = to_rgba(shades["shade10"])
        tokens[f"colorStatus{color}Foreground2"] = to_rgba(shades["shade30"])
        tokens[f"colorStatus{color}Foreground3"] = to_rgba(shades["primary"])
        tokens[f"colorStatus{color}ForegroundInverted"] = to_rgba(shades["tint30"])
        tokens[f"colorStatus{color}BorderActive"] = to_rgba(shades["primary"])
        tokens[f"colorStatus{color}Border1"] = to_rgba(shades["tint40"])
        tokens[f"colorStatus{color}Border2"] = to_rgba(shades["primary"])

    danger = mapped_status[status_mapping["danger"]]
    warning = mapped_status[status_mapping["warning"]]
    tokens["colorStatusDangerBackground3Hover"] = to_rgba(danger["shade10"])
    tokens["colorStatusDangerBackground3Pressed"] = to_rgba(danger["shade20"])
    tokens["colorStatusWarningForeground1"] = to_rgba(warning["shade20"])
    tokens["colorStatusWarningForeground3"] = to_rgba(warning["shade20"])
    tokens["colorStatusWarningBorder2"] = to_rgba(warning["shade20"])

    return tokens


def format_color(rgba):
    return ", ".join(f"{c:.6f}f" for c in rgba)


def main():
    colors_src = read_text("global", "colors.js")
    colors = parse_exported_objects(colors_src)

    brand_src = read_text("global", "brandColors.js")
    brand_colors = parse_exported_objects(brand_src)
    brand_web = OrderedDict(sorted(((k, v) for k, v in brand_colors["brandWeb"].items()), key=lambda x: int(x[0])))

    ctx = {
        name: colors[name]
        for name in (
            "grey",
            "whiteAlpha",
            "blackAlpha",
            "grey10Alpha",
            "grey12Alpha",
            "grey14Alpha",
            "red",
            "green",
            "darkOrange",
            "yellow",
            "berry",
            "lightGreen",
            "marigold",
            "darkRed",
            "cranberry",
            "pumpkin",
            "peach",
            "gold",
            "brass",
            "brown",
            "forest",
            "seafoam",
            "darkGreen",
            "lightTeal",
            "teal",
            "steel",
            "blue",
            "royalBlue",
            "cornflower",
            "navy",
            "lavender",
            "purple",
            "grape",
            "lilac",
            "pink",
            "magenta",
            "plum",
            "beige",
            "mink",
            "platinum",
            "anchor",
            "orange",
        )
    }
    ctx["white"] = colors["white"]
    ctx["black"] = colors["black"]

    light_src = read_text("alias", "lightColor.js")
    dark_src = read_text("alias", "darkColor.js")
    color_tokens = build_color_tokens(light_src, dark_src, ctx, brand_web)

    shared_names_src = read_text("sharedColorNames.js")
    status_names = literal_list(shared_names_src, "statusSharedColorNames")
    persona_names = literal_list(shared_names_src, "personaSharedColorNames")
    mapped_status_names = literal_list(shared_names_src, "mappedStatusColorNames")

    status_mapping_src = read_text("statusColorMapping.js")
    status_mapping = parse_exported_objects(status_mapping_src)["statusColorMapping"]

    palette_tokens = build_palette_tokens(ctx, status_names, persona_names, status_mapping, mapped_status_names)

    token_names = list(color_tokens.keys()) + list(palette_tokens.keys())

    print("// Generated by tools/generate_fluent_web_data.py")
    print("// Do not edit manually.\n")

    print("#ifdef FLUENT_WEB_INCLUDE_TOKEN_LIST")
    print("#undef FLUENT_WEB_INCLUDE_TOKEN_LIST\n")
    print("#define FLUENT_WEB_COLOR_TOKENS(M) \\")
    for idx, name in enumerate(token_names):
        suffix = " \\" if idx != len(token_names) - 1 else ""
        print(f"    M({name}){suffix}")
    print("\n#endif\n")

    print("#ifdef FLUENT_WEB_INCLUDE_TOKEN_DATA")
    print("#undef FLUENT_WEB_INCLUDE_TOKEN_DATA\n")

    print("struct FluentWebColorTokenEntry {")
    print("    const char *name;")
    print("    float light[4];")
    print("    float dark[4];")
    print("};")
    print()
    print("static const FluentWebColorTokenEntry kFluentWebColorTokenData[] = {")
    for name in token_names:
        if name in color_tokens:
            light_rgba, dark_rgba = color_tokens[name]
        else:
            rgba = palette_tokens[name]
            light_rgba = dark_rgba = rgba
        print(
            f'    {{"{name}", {{ {format_color(light_rgba)} }}, {{ {format_color(dark_rgba)} }}}},'
        )
    print("};\n#endif")


if __name__ == "__main__":
    main()
