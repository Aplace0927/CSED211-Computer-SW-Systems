# CSED211 - Lab2 (datalab-floating-point) Report

**20220140 Taeyeon Kim**

[toc]



---

## `negate()`

```c
int negate(int x) {
  return (~x + 1);
}
```

### Explanation

2's complement와 1's complement의 관계를 사용하여 나타낼 수 있다.

---

## `isLess()`

```c
int isLess(int x, int y) {
  return ((x >> 31) & !(y >> 31)) | !!((x + (~y + 1)) >> 31);
}
```

### Explanation

`negate()`에서 사용한 것과 유사하게, `x - y`의 sign bit가 0인지 1인지 반환한다.
여기서, `x - y`에서 Overflow가 일어나지 않도록 `operator|`의 좌측 항에는 overflow를 방지하는 term을 넣었다.

---

## `float_abs()`

```c
int addOK(int x, int y) {
  return uf & (~((!(!(~(uf >> 23) & 0xFF) && (uf << 9))) << 31));
}
```

### Idea of implementation

#### Detecting `NaN`

`NaN`이 되는 경우는 exponent part가 `0b11111111`의 bit pattern을 가져야 하고, mantissa part가 `0b0`이 아니어야 한다. (이 경우 입력받은 `uf`를 그대로 반환)

다음과 같은 의사 코드로 나타낼 수 있다.

> * 23~30th Bit 파악
>   * `0xFF` 이면서 mantissa가 `0`이 아닌 경우
>     - `uf` 반환
>   * 그 이외
>     - `uf & 0x7FFFFFFF` 반환 (sign bit를 0으로 채운다)

0xFF를 감지하기 위해 efficient한 코드를 작성하면 다음과 같다.
```c
(uf >> 23) & 0xFF   // 23 ~ 30 bit 추출
~(uf >> 23) & 0xFF  // 23 ~ 30 bit 추출 후 bit flip -> 0xFF인 경우에 0

!(~(uf >> 23) & 0xFF) // exponent bit 추출 후 0xFF인 경우 1
```


최종 코드의 부분 별 설명은 다음과 같다.

```c
uf & (~((!(!(~(uf >> 23) & 0xFF) && (uf << 9))) << 31));
          <-------------------->    <------->
             Exponent == 0xFF     Mantissa != 0
        <-------------------------------------------->
        Bitmask: flipped form (0x10000000, 0x00000000)
      <------------------------------------------------>
            Desired bitmask (0x7FFFFFFF, 0xFFFFFFFF)       
```

---

## `float_twice()`

```c
unsigned float_twice(unsigned uf) {
   unsigned sign = uf >> 31;
   unsigned mantissa = (uf << 9) >> 9;
   unsigned exponent = (uf >> 23) & 0xFF;
   unsigned mantissa_of = (mantissa << 1) >> 23 + (exponent != 0);

   if (exponent == 0xFF) // INF, NaN * 2 -> INF, NaN
   {
      return uf;
   }

   return (sign) << 31 | (exponent + mantissa_of + (exponent != 0)) << 23 | ((mantissa << !exponent)); 
}
```

### Idea of implementation

#### Returning itself: `NaN`, `INF`

`NaN`와 `INF`는 `float_twice()`를 했을 때 그대로 `NaN`, `INF`가 나온다.
> * `[exponent part] == 0xFF`인 경우 그대로 반환. 

작성한 코드의 다음 부분이 위 logic을 수행한다.
```c
if (exponent == 0xFF)
{
  return uf;
}
```

#### Multiply by 2: Normalized vs. Denormalized
> * Normalized Case `exponent != 0`
>   - exponent part를 증가시킨다.
> * Denormalized Case `exponent == 0`
>   - mantissa part를 bitshift left 하되, **Overflow가 일어나는 경우 exponent를 증가시킨다**

작성한 코드의 다음 부분이 위 logic을 수행하며, 설명을 첨부했다.
```c
return \
	(sign) << 31 | 
	<---------->            
  	   Sign Bit 
 
	(exponent + mantissa_of + (exponent != 0)) << 23 | \
    <------->   <--------->   <-------------->
       Exp.     Mantissa OF     Exp. Increase
    		  (from Denorm.)	  (Norm.)
    
    ((mantissa << !exponent));
	 <-------->   <-------->
      Mantissa    Mantissa SHL
       		       (Denorm.)
```

---

## `float_i2f()`

```c
unsigned float_i2f(int x) {   
   int sign;
   unsigned abs, cpy_abs, exp, mantissa, guard, remainder;
   
   if(!x){
      return x;
   }

   sign = (x >> 31);

   exp = 0x7F;
   abs = sign ? -x : x;
   cpy_abs = abs;

   while (cpy_abs > 0b1)
   {
      cpy_abs >>= 1;
      exp++;
   }

   mantissa = (abs << (0x9f - exp) ) >> 9;
   guard = exp >= 0x97 ? (abs >> (exp - 0x97)) & 0b1 : 0;
   remainder = exp >= 0x98 ? (abs << (0xb7 - exp)) != 0 : 0;
    
   mantissa += !((!(mantissa & 0b1) & guard & !remainder) || !guard); // ~[[2], [0,1,4,5]] -> [3, 6, 7]

   return ((sign << 31) | (exp << 23)) + mantissa;
}
```
### Idea of implmentation

Denormalized된 `0`의 경우, 예외 케이스로 간주하여 함수 초기에 반환해 주었다. (`if(!x) {return x;}`)

#### Bit counting (for exponent)

가장 높은 bit를 찾기 위해, 주어진 integer를 복사하여 bitshift right하며 0이 될 때 까지 exponent를 증가시킨다.

```c
exp = 0x7F;
abs = sign ? -x : x;
cpy_abs = abs;

while (cpy_abs > 0b1)
{
  cpy_abs >>= 1;
  exp++;
}
```

~~이는 어셈블리에서 `bsr`, MSVC에서는 `_BitScanReverse()` 또는 `_BitScanForward()` 등으로 바로 사용할 수 있다~~ 

#### Mantissa - Mantissa *body*

가장 처음 나타나는 1 뒤로 나타나는 23개의 bit를 추출하면 된다.
처음으로 1이 나타나는 위치가 `exp - 0x7F`이고, 총 bit의 개수는 4byte = 32bit이기 때문에 다음과 같이 Bit operation을 진행한다.

```c
(x << (0x20 - (exp - 0x7F))) >> 9;
      <--->   <---------->   <-->
sizeof(int)  Mantissa start  Save high 23bit(32 - 23)
```

#### Mantissa - *Guard* Bit

Guard bit는 Mantissa bit가 끝난 이후로 나타나며, Mantissa가 23bit 이기 때문에 Most significant 1이 `1 << 23` 혹은 그보다 큰 위치에 있어야 한다.

> * `exp > (24 + 0x7F)` 인 경우
>  - `(exp - (24 + 0x7F))` 만큼 bitshift right를 하여 least significant bit을 가져온다.
> * 그 외의 경우
>  - Guard bit는 `0` (Round-up 방지)
>

```c
guard = exp >= 0x97 ? (abs >> (exp - 0x97)) & 0b1 : 0;
        <--------->   <------------------->   <->   |
         Condition        Bitshift right      LSB   Else
```
#### Mantissa - Remainders

Remainder는 Guard bit가 `exp >= 24 + 0x7F` 이상에서 유효했던 것 처럼, 그 아래에 있는 Remainder는 `exp >= 25 + 0x7F`에서 유효하다.

Guard **bit** 는 아래에 있는 모든 bit를 `operator|` 하였을때 나오는 결과이기 때문에, Guard가 0인지 0이 아닌지 파악하여 boolean operation으로 반환할 수 있다.

> `exp = 0x7F + 25`일 때 remainder는 하위 1bit (0),
> `exp = 0x7F + 26`일 때 remainder는 하위 2bit (0, 1), ...

위와 같이 Bit pattern이 나타나기 떄문에 `31 - (exp - (0x7F + 25))` = `(0xB7 - exp)`번 right shift를 했을 때:
* 이 값이 `0`이면 remainder bit pattern이 `0`인 경우,
* 이 값이 `0`이 아니면 remainder bit pattern이 `1`인 경우이다. 

```c
remainder = exp >= 0x98 ? (abs << (0xb7 - exp)) != 0 : 0;
            <--------->   <-------------------> <-->   |
             Condition    Remainder bit pattern bool   Else
```

#### Mantissa - Round-up

(Mantissa의 Least significant bit), (Guard bit), (Remainder) 순으로 나타냈을 때 모든 8개의 경우에 대한 Round-up 진리표는 다음과 같다. *(편의를 위해 각각 L, G, R으로 축약함)*

|L|G|R|**Round**|
|---|---|---|---|
|0|0|0|0|
|0|0|1|0|
|0|1|0|0|
|0|1|1|**1**|
|1|0|0|0|
|1|0|1|0|
|1|1|0|**1**|
|1|1|1|**1**|

이를 표현하는데 필요한 Bit를 최소화하기 위해 Karnaugh-map을 사용하여 단순화하였다.

|L \ (G,R)|00|01|11|10|
|--|--|--|--|--|
|**0**|<span style="color:cyan">0</span>|<span style="color:cyan">0</span>|**1**|<span style="color:magenta">0</span>|
|**1**|<span style="color:cyan">0</span>|<span style="color:cyan">0</span>|**1**|**1**|

편하게 표현하기 위해, 여사건의 여사건으로 표현하였다.

**<span style="color:cyan">청록색</span>** 부분은 `!G` 로 나타낼 수 있으며, **<span style="color:magenta">자주색</span>** 부분은 `!L & G & !R` 로 나타낼 수 있다.

마지막으로 C-language로 옮겨서 다음과 같이 완성했다.

```c
mantissa += !((!(mantissa & 0b1) & guard & !remainder) || !guard);
            |  <------------------------------------->    <----->
            |           Magenta-colored part                Cyan
      Complement            
```

### Conclusion

각 부분을 `operator<<` 및 `operator|`, `operator+`로 붙여 최총적으로 함수를 완성했다.


**Mantissa에서 Overflow가 일어나는 경우**, 자동적으로 Exponent에 더해지도록 `operator+` 를 사용해 mantissa part를 붙였다.
```c
return ((sign << 31) | (exp << 23)) + mantissa;
        <---------->   <--------->  | <------>
          Sign part     Exp. part   | Mantissa
                                    |
                              Overflow handling
```

---

## `float_f2i()`

```c
int float_f2i(unsigned uf) {
   unsigned sign, norm_mantissa;
   int exponent, result;
   sign = uf >> 31;
   norm_mantissa = 1 << 23 | (uf << 9) >> 9;
   exponent = ((uf >> 23) & 0xFF) - 0x7F;
   result = (1U << 31);
   
   /* Un-representable with Integer (INF, NaN)*/
   if (exponent >= 31) {
      return result;
   }

   /* Smaller than 1 is 0*/
   if (exponent < 0){
      return 0;
   }

   result = exponent < 23 ? norm_mantissa >> (23 - exponent) : norm_mantissa << (exponent - 23);

   return sign ? -result : result;
}
```

### Idea of implementation

#### Unrepresentable case
Integer type으로 나타낼 수 없는 floating-point의 표현에는 `NaN`과 `INF`가 있으며, 이 둘은 모두 `exp == (0x7F + 0xFF)` 이다. 이 외에 32-bit integer가 나타낼 수 있는 한계 이상(`exp >= 0x7F + 31`)의 경우도 표현하지 못한다.

따라서 이 경우 요구한 대로 `0x80000000 == (1U << 31)`을 반환하며, 아래의 코드에서 이 경우를 처리하게 된다.
```c
exponent = ((uf >> 23) & 0xFF) - 0x7F;
if (exponent >= 31) {
      return result;
}
```

#### Too small
나타낸 결과가 0 ~ 1 사이의 소수인 경우 바로 0을 반환한다. 이 경우는 `exp < 0x7F`인 경우이며, 아래의 코드에서 이를 처리해낸다.
```c
exponent = ((uf >> 23) & 0xFF) - 0x7F;
if (exponent < 0){
      return 0;
}
```

#### Else
Mantissa의 Pattern을 얻어 와, Exponent만큼 Bitshift를 진행한다.

`float`의 mantissa presicion은 23bit이기 때문에:

* `exp < (0x7F + 23)`인 경우:
  * Bitshift left로 bit pattern을 least significant bit까지 채운다.
* `exp >= (0x7F + 23)`인 경우:
  * 상위 23bit의 pattern을 적용한 이후, bitshift right하여 2의 거듭제곱 꼴로 만들어 채운다.

이후 최종적으로 sign bit를 고려하여 반환했고, 코드를 완성했다.
```c
result = \
    exponent < 23 ? \
    <----------->   
       Condition

    norm_mantissa >> (23 - exponent) : norm_mantissa << (exponent - 23);
	<------------------------------>   <------------------------------>
      Fill from Least signfic. bit     Pad zero from L.S.Bit to exponent
        
return sign ? -result : result;
```