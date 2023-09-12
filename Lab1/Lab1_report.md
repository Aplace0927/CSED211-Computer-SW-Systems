# CSED211 - Lab1 (datalab) Report

20220140 Taeyeon Kim


## `bitNor()`

```c
int bitNor(int x, int y) {
  return (~x & ~y);
}
```

### Explanation

De Morgan의 법칙에 따라 `~(x|y)`는 `(~x&~y)`와 동치이다.


## `isZero()`

```c
int isZero(int x) {
    return !x;
}
```

### Explanation

Operator `!` 는 integer가 0인 경우 `1`, 아닌 경우 `0` 을 반환한다.


## `addOK()`

```c
int addOK(int x, int y) {
  int sx = ((x >> 31) & 0b1);
  int sy = ((y >> 31) & 0b1);
  int sr = (((x + y) >> 31) & 0b1);
  return !(!(sx ^ sy) & (sy ^ sr));
}
```

### Idea of implementation

#### In case of overflow

Overflow가 일어날 조건은 다음과 같으며, 이 여사건이 `addOK()`를 만족시키는 경우이다.
> * `x` 와 `y` 의 부호가 같고, 더한 값의 결과 부호는 `x` (혹은 `y`)의 부호와 달라야 한다.

이를 식으로 옮기면 다음과 같다.
```c
(x >> 31) & 0b1;        // sign of x
(y >> 31) & 0b1;        // sign of y
((x + y) >> 31) & 0b1;  // sign of (x + y)
    
/* (sign of x) and (sign of y) should same. */
!(((x >> 31) & 0b1) ^ ((y >> 31) & 0b1));


/* (sign of y) and (sign of x + y) should diffrent. */
((y >> 31) & 0b1) ^ (((x + y) >> 31) & 0b1);

/* `addOK()` if both condition are NOT satisfied. */
!((!(((x >> 31) & 0b1) ^ ((y >> 31) & 0b1))) & (((y >> 31) & 0b1) ^ (((x + y) >> 31) & 0b1)));
```

여기서 각 부분을 추출 한 파트를 (가독성 및 재사용을 위해) 변수로 선언하여 결과와 같은 코드를 만들어 냈다.


## `absVal()`

```c
int absVal(int x) {
  return ((~x + 1) & (x >> 31)) | (x & ~(x >> 31));
}
```

### Idea of implementation

#### Extracting sign bit

절댓값을 추출하기 위해서는 다음과 같은 과정이 진행되어야 한다.
> * 부호 파악
>   - 양수인 경우 그대로 반환
>   - 음수인 경우 부호를 바꾸어 반환


이를 비트 연산을 통해 구현하면,
> * 부호 파악: `(x >> 31) & 0b1`
>   - `0` 인 경우 그대로 반환.
>   - `1` 인 경우 `~x + 1` 을 반환.


이를 해결하기 위해 각 경우를 위한 **Mutual exclusive한 Bitmask를 생성하고자 했다.**
C language의 `operator>>` 는 arithmetic shift이기 때문에, MSB인 sign bit를 복사하는 방식으로 채운다.

따라서 `x >> 31` 을 통해 모든 bit를 sign bit로 채운 bitmask constant를 생성할 수 있으며, Mutual exclusive한 bitmask를 생성하기 위해 `~(x >> 31)` 를 사용할 수 있다. 이를 다시 정리하면,

#### Applying mutual exclusive bitmask

* 부호 파악 및 Mutual exclusive bitmask 생성: `(x >> 31)` and `~(x >> 31)`
   - 양수인 경우 `(0xFFFFFFFF & x) == ~(x >> 31) & x` 반환.
   - 음수인 경우 `(0xFFFFFFFF & x) == (x >> 31) & (~x + 1)` 반환.

최종적으로 상술한 설명을 코드로 옮겨 `absVal()` 을 완성하였다.

## `logicalShift()`

```c
int logicalShift(int x, int n) {
  return ((x >> n) & (~((~0 + !n) << ((~n + 1) & 0x1F))));
}
```
### Idea of implmentation

#### SHL instead of SHR

logical shift는 기본적으로 C language에서 제공하는 `operator>>` 를 사용하여 arithmetic shift를 진행하되, sign bit를 복사하는 것이 아닌 shift right를 한 만큼 `0`으로 채워야 한다. 따라서  **`0 (n bit) + 1 ((32 - n) bit)` 형태의 bitmask가 필요하다.**

`(unsigned) 0xFFFFFFFF`를 *n*회 right shift하여 구현할 수 있으나, `unsigned` type을 사용할 수 없기 때문에 `(int) 0xFFFFFFFF`를 *(32 - n)* 회 **left shift하고, 이를 negate하여 구현하고자 했다.**

#### SHL counting bitmask

이를 위해 `(32 - n)` 을 구현하여 `((~n + 1) & 0x1F)`을 하였으나, `n == 0` 인 경우 `0xFFFFFFFF` 를 `0`번 shift left 이후 negate하여 `0x00000000` 이 나오게 되었다. 따라서 `n == 0` 경우엔 모든 비트를 뒤집을 수 있도록 초기 비트마스크를 `(~0 + !n)` 으로 설정했다.

다음과 같이 진행된다.
```c
/* Case #1: n == 0 */
  ((x >> n) & (~((~0 + !n) << ((~n + 1) & 0x1F))));
= ((x >> n) & (~((~0 + 1) << ((~0 + 1) & 0x1F))));
= ((x >> n) & (~(0x00000000 << (0 & 0x1F))));
= ((x >> n) & (~(0x00000000)))
= (x >> n) & 0xFFFFFFFF


/* Case #2: n != 0 */
  ((x >> n) & (~((~0 + !n) << ((~n + 1) & 0x1F))));
= ((x >> n) & (~((~0) << (32 - n))));
= ((x >> n) & (~((0xFFFFFFFF) << (32 - n))));
= ((x >> n) & 0b0...(n times)..01...((32 - n) times)...1)
```

이를 통해 `logicalShift()` 의 코드를 완성했다.