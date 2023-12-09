import os
import sys
import difflib
from colorama import Fore, Style
from pprint import pprint

SHELL_REF = "./tshref"
SHELL_USR = "./tsh"

TRACE_CNT = 16
TRACES = [f"trace{id:02d}.txt" for id in range(1, TRACE_CNT + 1)]

THRESHOLD = 0.9

D = difflib.Differ()


def transpose_print(ratio: float, st: list[str]) -> None:
    diff = [ch[0] for ch in st]
    char = [ch[-1] for ch in st]

    _diff = []
    _char = []

    for d, c in zip(diff, char):
        if d == "-":
            diffrent = True
            _char.append(f"{Fore.RED}{c}{Style.RESET_ALL}")
            _diff.append(f"{Fore.RED}-{Style.RESET_ALL}")
        elif d == "+":
            diffrent = True
            _char.append(f"{Fore.GREEN}{c}{Style.RESET_ALL}")
            _diff.append(f"{Fore.GREEN}+{Style.RESET_ALL}")
        else:
            _char.append(c)
            _diff.append(d)

    print(f"{ratio:.04f} : {''.join(_diff)}\n       | {''.join(_char)}")


if __name__ == "__main__":
    print(f"threshold = {THRESHOLD}")
    print("-" * 25)
    for trace_file in TRACES:
        diff_lines = 0

        os.system(f'./sdriver.pl -v -t {trace_file} -s {SHELL_REF} -a "-p" > REF.log')
        os.system(f'./sdriver.pl -v -t {trace_file} -s {SHELL_USR} -a "-p" > USR.log')

        ref_str = ""
        usr_str = ""

        with open("REF.log", "r") as ref_f:
            ref_str = ref_f.read().splitlines(keepends=True)

        with open("USR.log", "r") as usr_f:
            usr_str = usr_f.read().splitlines(keepends=True)

        assert len(ref_str) == len(usr_str)

        for ref_line, usr_line in zip(ref_str, usr_str):
            if (
                rat := difflib.SequenceMatcher(None, ref_line, usr_line).ratio()
            ) < THRESHOLD:
                transpose_print(rat, list(D.compare(ref_line, usr_line)))
                diff_lines += 1

        print(f"[{trace_file}] : {diff_lines} diffs")
