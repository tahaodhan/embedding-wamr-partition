#!/usr/bin/env python3
#
# Copyright (C) 2019 Intel Corporation.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
"""
It is used to process *out.folded* file generated by [FlameGraph](https://github.com/brendangregg/FlameGraph).

- translate jitted function names, which are in a form like `aot_func#N` or `[module name]#aot_func#N`, into corresponding names in a name section in .wasm
- divide the translated functions into different modules if the module name is specified in the symbol

Usage:

After
``` bash
# collect profiling data in perf.data

$ perf script -i perf.data > out.perf

$ ./FlameGraph/stackcollapse-perf.pl out.perf > out.folded
```

Use this script to translate the function names in out.folded

```
$ python translate_wasm_function_name.py --wabt_home <wabt-installation> --folded out.folded <.wasm>
# out.folded -> out.folded.translated
```

"""

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
from typing import Dict, List


# parse arguments like "foo=bar,fiz=biz" into a dictatory {foo:bar,fiz=biz}
class ParseKVArgs(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, dict())
        for value in values.split(","):
            k, v = value.split("=")
            getattr(namespace, self.dest)[k] = v


def calculate_import_function_count(
    wasm_objdump_bin: Path, module_names: Dict[str, Path]
) -> Dict[str, int]:
    """
    for every wasm file in <module_names>, calculate the number of functions in the import section.

    using "<wasm_objdump_bin> -j Import -x <wasm_file>"
    """

    assert wasm_objdump_bin.exists()

    import_function_counts = {}
    for module_name, wasm_path in module_names.items():
        assert wasm_path.exists()
        command = f"{wasm_objdump_bin} -j Import -x {wasm_path}"
        p = subprocess.run(
            shlex.split(command),
            capture_output=True,
            check=False,
            text=True,
            universal_newlines=True,
        )

        if p.stderr:
            print("No content in import section")
            import_function_counts[module_name] = 0
            continue

        import_function_count = 0
        for line in p.stdout.split(os.linesep):
            line = line.strip()

            if not line:
                continue

            if not " func" in line:
                continue

            m = re.search(r"^-\s+func", line)
            assert m

            import_function_count += 1

        # print(f"! there are {import_function_count} import function in {module_name}")
        import_function_counts[module_name] = import_function_count

    return import_function_counts


def collect_name_section_content(
    wasm_objdump_bin: Path, module_names: Dict[str, Path]
) -> Dict[str, Dict[int, str]]:
    """
    for every wasm file in <module_names>, get the content of name section.

    execute "wasm_objdump_bin -j name -x wasm_file"
    """
    assert wasm_objdump_bin.exists()

    name_sections = {}
    for module_name, wasm_path in module_names.items():
        assert wasm_path.exists()
        command = f"{wasm_objdump_bin} -j name -x {wasm_path}"
        p = subprocess.run(
            shlex.split(command),
            capture_output=True,
            check=False,
            text=True,
            universal_newlines=True,
        )

        if p.stderr:
            print("No content in name section")
            name_sections[module_name] = {}
            continue

        name_section = {}
        for line in p.stdout.split(os.linesep):
            line = line.strip()

            if not line:
                continue

            if not " func" in line:
                continue

            # - func[N] <__imported_wasi_snapshot_preview1_fd_close>
            m = re.match(r"- func\[(\d+)\] <(.+)>", line)
            assert m

            func_index, func_name = m.groups()
            name_section.update({int(func_index): func_name})

        name_sections[module_name] = name_section

    return name_sections


def is_stack_check_mode(folded: Path) -> bool:
    """
    check if there is a function name looks like "aot_func_internal#N", it means that WAMR adds a stack check function before the original function.
    """
    with open(folded, "rt", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if "aot_func_internal" in line:
                return True
    return False


def replace_function_name(
    import_function_counts: Dict[str, int],
    name_sections: Dict[str, Dict[int, str]],
    folded_in: Path,
    module_names: Dict[str, Path],
) -> None:
    """
    read content in <folded_in>.  every line contains symbols which are separated by ";".

    Usually, all jitted functions are in the form of "aot_func#N". N is its function index. Use the index to find the corresponding function name in the name section.

    if there is a function name looks like "aot_func_internal#N", it means that WAMR adds a stack check function before the original function.
    In this case, "aot_func#N" should be translated with "_precheck" as a suffix and "aot_func_internal#N" should be treated as the original one
    """

    assert folded_in.exists(), f"{folded_in} doesn't exist"

    stack_check_mode = is_stack_check_mode(folded_in)

    # every wasm has a translated out.folded, like out.<module_name>.folded.translated
    folded_out_files = {}
    for module_name in module_names.keys():
        wasm_folded_out_path = folded_in.with_suffix(f".{module_name}.translated")
        print(f"-> write into {wasm_folded_out_path}")
        folded_out_files[module_name] = wasm_folded_out_path.open(
            "wt", encoding="utf-8"
        )
    # Plus a default translated out.folded
    default_folded_out_path = folded_in.with_suffix(".translated")
    print(f"-> write into {default_folded_out_path}")
    default_folded_out = default_folded_out_path.open("wt", encoding="utf-8")

    with folded_in.open("rt", encoding="utf-8") as f_in:
        for line in f_in:
            line = line.strip()

            m = re.match(r"(.*) (\d+)", line)
            assert m
            syms, samples = m.groups()

            new_line = []
            last_function_module_name = ""
            for sym in syms.split(";"):
                if not "aot_func" in sym:
                    new_line.append(sym)
                    continue

                # [module_name]#aot_func#N or aot_func#N
                splitted = sym.split("#")
                module_name = "" if splitted[0] == "aot_func" else splitted[0]
                # remove [ and ]
                module_name = module_name[1:-1]

                if len(module_name) == 0 and len(module_names) > 1:
                    raise RuntimeError(
                        f"❌ {sym} doesn't have a module name, but there are multiple wasm files"
                    )

                if not module_name in module_names:
                    raise RuntimeError(
                        f"❌ can't find corresponds wasm file for {module_name}"
                    )

                last_function_module_name = module_name

                func_idx = int(splitted[-1])
                # adjust index
                func_idx = func_idx + import_function_counts[module_name]

                # print(f"🔍 {module_name} {splitted[1]} {func_idx}")

                if func_idx in name_sections[module_name]:
                    if len(module_name) > 0:
                        wasm_func_name = f"[Wasm] [{module_name}] {name_sections[module_name][func_idx]}"
                    else:
                        wasm_func_name = (
                            f"[Wasm] {name_sections[module_name][func_idx]}"
                        )
                else:
                    if len(module_name) > 0:
                        wasm_func_name = f"[Wasm] [{module_name}] func[{func_idx}]"
                    else:
                        wasm_func_name = f"[Wasm] func[{func_idx}]"

                if stack_check_mode:
                    # aot_func_internal -> xxx
                    # aot_func --> xxx_precheck
                    if "aot_func" == splitted[1]:
                        wasm_func_name += "_precheck"

                new_line.append(wasm_func_name)

            line = ";".join(new_line)
            line += f" {samples}"

            # always write into the default output
            default_folded_out.write(line + os.linesep)
            # based on the module name of last function, write into the corresponding output
            if len(last_function_module_name) > 0:
                folded_out_files[last_function_module_name].write(line + os.linesep)

    default_folded_out.close()
    for f in folded_out_files.values():
        f.close()


def main(wabt_home: str, folded: str, module_names: Dict[str, Path]) -> None:
    wabt_home = Path(wabt_home)
    assert wabt_home.exists()

    folded = Path(folded)
    assert folded.exists()

    wasm_objdump_bin = wabt_home.joinpath("bin", "wasm-objdump")
    import_function_counts = calculate_import_function_count(
        wasm_objdump_bin, module_names
    )

    name_sections = collect_name_section_content(wasm_objdump_bin, module_names)

    replace_function_name(import_function_counts, name_sections, folded, module_names)


if __name__ == "__main__":
    argparse = argparse.ArgumentParser()
    argparse.add_argument(
        "--wabt_home", required=True, help="wabt home, like /opt/wabt-1.0.33"
    )
    argparse.add_argument(
        "--wasm",
        action="append",
        default=[],
        help="wasm files for profiling before. like --wasm apple.wasm --wasm banana.wasm",
    )
    argparse.add_argument(
        "--wasm_names",
        action=ParseKVArgs,
        default={},
        metavar="module_name=wasm_file, ...",
        help="multiple wasm files and their module names, like a=apple.wasm,b=banana.wasm,c=cake.wasm",
    )
    argparse.add_argument(
        "folded_file",
        help="a out.folded generated by flamegraph/stackcollapse-perf.pl",
    )

    args = argparse.parse_args()

    if not args.wasm and not args.wasm_names:
        print("Please specify wasm files with either --wasm or --wasm_names")
        exit(1)

    # - only one wasm file. And there is no [module name] in out.folded
    # - multiple wasm files. via `--wasm X --wasm Y --wasm Z`. And there is [module name] in out.folded. use the basename of wasm as the module name
    # - multiple wasm files. via `--wasm_names X=x,Y=y,Z=z`. And there is [module name] in out.folded. use the specified module name
    module_names = {}
    if args.wasm_names:
        for name, wasm_path in args.wasm_names.items():
            module_names[name] = Path(wasm_path)
    else:
        # use the basename of wasm as the module name
        for wasm in args.wasm:
            wasm_path = Path(wasm)
            module_names[wasm_path.stem] = wasm_path

    main(args.wabt_home, args.folded_file, module_names)
