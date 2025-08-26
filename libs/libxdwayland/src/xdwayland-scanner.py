#!/usr/bin/python3

from io import TextIOWrapper
import xml.etree.ElementTree as ET
import os
import sys


def generate_comment(text: str) -> str:
    ret = []
    text = text.strip()
    lines = text.splitlines()
    line_c = len(lines)

    for i, line in enumerate(lines):
        line = line.strip()

        if i == 0:
            row = f"/* {line}"

        else:
            row = f"* {line}"

        if i == line_c - 1:
            row += f" */"

        ret.append(row)

    return "\n".join(ret)


def generate_requests(interface_name: str, cur: ET.Element, *, header: bool) -> str:
    requests = ""

    for i, request in enumerate(cur.findall("./request")):
        request_name = request.get("name")
        request_description = request.find("./description")

        if header and request_description is not None and request_description.text:
            method = generate_comment(request_description.get("summary", "") + "\n" + request_description.text) + "\n"
        else:
            method = ""

        method += f"void xd{interface_name}_{request_name}(xdwl_proxy *proxy, "

        args = []

        if interface_name == "wl_registry" and request_name == "bind":
            method += "uint32_t _name, const char *_interface, uint32_t _version, uint32_t _new_id, "
            args = ["_name", "_interface", "_version", "_new_id"]

        else:
            for arg in request.findall("./arg"):
                arg_name = "_" + arg.get("name", "")
                arg_type = arg.get("type")
                args.append(arg_name)

                match arg_type:
                    case "int" | "enum":
                        method += f"int32_t {arg_name}"

                    case "uint" | "new_id" | "object":
                        method += f"uint32_t {arg_name}"

                    case "fd":
                        method += f"int {arg_name}"

                    case "fixed":
                        method += f"float {arg_name}"

                    case "string":
                        method += f"const char *{arg_name}"

                method += ", "

        method = method.rstrip(", ")
        method += ")"

        if not header:
            method += " {\n"
            if args:
                method += f'    xdwl_send_request(proxy, "{interface_name}", {i}, {len(args)}, {", ".join(args)});\n'

            else:
                method += f'    xdwl_send_request(proxy, "{interface_name}", {i}, 0);\n'
            method += "}"

        method += ";"

        requests += method + "\n\n"

    return requests


def generate_add_listener(interface_name: str, *, header: bool) -> str:
    if header:
        listener = f"void xd{interface_name}_add_listener(xdwl_proxy *proxy, struct xd{interface_name} *interface, void *user_data);"

    else:
        listener = f"""void xd{interface_name}_add_listener(xdwl_proxy *proxy, struct xd{interface_name} *interface, void *user_data) {{
      xdwl_add_listener(proxy, \"{interface_name}\", interface, user_data);
    }};"""

    return listener


def generate_events(interface_name: str, cur: ET.Element, *, header: bool) -> str | None:
    if cur.find("./event") is not None:
        if not header:
            return f"struct xd{interface_name};"

        struct = f"""struct xd{interface_name} {{
"""

        for event in cur.findall("./event"):
            event_description = event.find("./description")
            if event_description is not None and event_description.text:
                struct += "\n" + generate_comment(event_description.get("summary", "") + "\n" + event_description.text) + "\n"

            struct += f"  xdwl_event_handler(*{event.get('name')});\n\n"

        struct += "};"

        return struct


def generate_enum(interface_name: str, cur: ET.Element) -> str | None:
    enum_t = ""

    for enum in cur.findall("./enum"):
        enum_name = enum.get("name")
        enum_base_name = f"xd{interface_name}_{enum_name}"

        enum_t += f"""enum {enum_base_name} {{
"""

        for entry in enum.findall("./entry"):
            entry_name = entry.get("name")
            assert entry_name
            entry_value = entry.get("value")
            assert entry_value

            entry_summary = entry.get("summary")

            if entry_summary:
                enum_t += "\n  " + generate_comment(entry_summary) + "\n"

            enum_t += f"  {enum_base_name.upper()}_{entry_name.upper()} = {entry_value},\n"

        enum_t += "};\n\n"

    return enum_t


def generate_interface(cur: ET.Element, h: TextIOWrapper, c: TextIOWrapper) -> None:
    interface_name = cur.get("name")
    assert interface_name

    interface_description = cur.find("./description")
    assert interface_description is not None and interface_description.text

    interface_description = generate_comment(interface_description.text)

    enum = generate_enum(interface_name, cur)
    if enum:
        h.write(enum + "\n")

    events_h = generate_events(interface_name, cur, header=True)
    events_c = generate_events(interface_name, cur, header=False)

    if events_h and events_c:
        h.write("\n" + events_h + "\n")
        h.write(generate_add_listener(interface_name, header=True) + "\n\n")

        c.write("\n" + events_c + "\n")
        c.write(generate_add_listener(interface_name, header=False) + "\n\n")

    requests_h = generate_requests(interface_name, cur, header=True)
    requests_c = generate_requests(interface_name, cur, header=False)

    if requests_h and requests_c:
        h.write("\n" + requests_h + "\n")
        c.write("\n" + requests_c + "\n")


def generate(input: str, output_path_base: str) -> None:
    tree = ET.parse(input)
    root = tree.getroot()

    output_h = output_path_base + ".h"
    output_c = output_path_base + ".c"

    h = open(output_h, "w")
    c = open(output_c, "w")

    h_guard = f"__XDWAYLAND_{os.path.basename(output_path_base).upper().replace('-', '_')}"
    h.write(
        f"""#ifndef {h_guard}
#define {h_guard}

"""
    )

    c.write(
        f"""#include "xdwayland-client.h"
#include "{os.path.basename(output_path_base)}.h"

"""
    )

    h.write(
        f"""#include "xdwayland-client.h"

"""
    )
    for interface in root.findall("./interface"):
        generate_interface(interface, h, c)

    h.write("\n#endif")

    h.close()
    c.close()


def main():
    if len(sys.argv) != 3:
        print("Invalid arguments")
        return

    path = sys.argv[1]
    path = os.path.abspath(path)

    output_path_base = sys.argv[2]

    generate(path, output_path_base)


main()
