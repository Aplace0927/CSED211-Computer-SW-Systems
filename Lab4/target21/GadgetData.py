import os
import hashlib

os.system("ROPgadget --binary rtarget > 'Gadgets.txt'")

with open("Gadgets.txt", "r") as g:
    gadget = g.read().splitlines()[2:-2]
    gadget = [gad.split(":") for gad in gadget]

    with open("Gadgets_Dict.txt", "w") as f:
        for addr, code in gadget:
            f.write(
                f"{hashlib.sha256(code.strip().encode('ascii')).hexdigest()} {int(addr.strip(), 16)}\n"
            )
