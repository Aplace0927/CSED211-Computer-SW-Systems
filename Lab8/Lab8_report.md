# CSED211 - Lab10 (shelllab) Report

**20220140 Taeyeon Kim**

[toc]

## Implemented Functions

### For debugging purpose

#### `check_error`

Macro function으로, 조건과 문자열이 주어졌을 때 해당 조건을 만족하면 `unix_error`를 호출하도록 설계했다. `tsh.c`를 작성하고, 디버깅하는 데 사용하였다.

```c
#define check_error(cond, from) ({ \
    if ((cond))                    \
    {                              \
        unix_error(from " error"); \
    }                              \
})
```



### Main functions

#### `eval`

먼저 주어진 argument를 parse한다. `parseline`의 Return value로 해당 Job이 background에서 실행될 지 foreground에서 실행될 지 반환된다. (만약 `parseline`의 결과가 빈 줄인 경우 반환한다.)

이후 [`builtin_cmd`](#`builtin_cmd`)를 통해 구현하도록 제시된 Command인 경우 해당 `builtin_cmd` 내에서 Command에 맞는 함수를 호출하여 수행한다.

만약 `builtin_cmd`에 없는 동작인 경우, 이후 `fork`를 수행하며, Child process와 Parent process로 나누어야 한다. 그러나 그 전에 **`fork` 과정에서 Process가 나뉘고, Signal handler가 생길 때 까지 Signal이 handling되지 않게 강제로 막아주기 위해 Signal을 block하는 과정이 필요하다. (CRITICAL SECTION)**

Signal을 Block하기 위해 다음과 같이 적용하였다.

* `sigemptyset`을 사용해 Empty signal set을 초기화하고, `sigaddset`을 사용하여 추가하도록 명시된 `SIGCHLD`와 `SIGINT`, `SIGTSTP`를 추가했다.
* 이후 `fork` 직전에 `sigprocmask(SIG_BLOCK, &sigset, NULL)`을 사용하여 Signal set을 적용했다.

이후 `fork`가 이루어지고, Child process와 Parent process로 나뉜 이후 동작은 다음과 같다. (`fork() < 0`인 경우의 Error handling을 처리했다. )

* **Child Process** (`process_id == 0`)
  * 앞서 막아 두었던 Signal을 모두 Unblock한다.
  * **새로 생긴 Child로부터 새롭게 계속 Child process가 생성될 수 있으며, 이 경우 같은 `pgroup`을 공유하게 된다. 이 때 가장 Child process의 가장 Ancestor를 `kill` 했을 때 해당 `pgroup`의 모든 Process에 적용될 수 있도록 `setpgid(0, 0)`을 사용하였다.**
  * `exceve(argv[0], (char *const *) argv, (char *const *)environ)`을 통해 `argv[0]`으로 주어진 새로운 프로그램을 Load하고, 실행하도록 현재 프로세스에 덮어쓴다.
  * 따라서 제대로 Load된 경우 Process의 실행 흐름이 바뀌어 아래에 있는 모든 줄이 실행되지 않는다. 아래에 있는 줄이 실행되는 유일한 경우는 `argv[0]`이 Invalid하여 `exceve`가 성공적으로 실행되지 않은 경우이며, 이 때 (포맷에 따라 )`%s : Command not found\n` 로 에러 메시지를 띄우고 `exit`으로 종료한다.
* **Parent Process** (`process_id > 0`)
  * 앞서 막아 두었던 Signal을 모두 Unblock한다.
  * 앞서 얻어낸 Job의 실행 위치 (background OR foreground)에 따라 `addjob`을 통해 수행할 job을 등록한다.
    * 만약 Background Job인 경우, 주어진 포맷에 따라 등록했음을 출력했다.
    * 만약 Foreground Job인 경우, 실행이 완료될 때 까지 Busy-wait을 통해 기다린다. ([`waitfg`](#`waitfg`) 사용)

#### `builtin_cmd`

구현하도록 주어진 4개의 Command를 구현했다.

* `quit`
  * `exit(0)`으로 해당 Process를 종료한다. 인자를 요구하지 않는다.
* `jobs`
  * 주어진 `listjob`을 호출해 전역적으로 선언된 Job의 list인 `jobs`의 각 Process 정보를 나타낸다. 인자를 요구하지 않는다.
* `bg` 및 `fg`
  * [`do_bgfg`](#`do_bgfg`)를 구현하여 해당 함수를 호출했다.  요구하는 인자는 1개로, `%JID` 혹은 `PID` 형식이다.

#### `do_bgfg`

크게 Parsing 하는 부분과 Job change하는 부분의 두 부분으로 나눌 수 있다.

* **Parsing**

  * Error case 처리: `argv[1]`이 없는 경우 (인자가 주어지지 않은 경우) / 주어진 `PID` 혹은 `%JID` 포맷에 맞지 않는 경우 (마지막 `else` 이후 처리됨)
    * 인자가 없거나 포맷에 맞지 않는 경우에 중단했다.
  * `argv[1][0] == '%` :  `%JID` 포맷인 경우
    * 다음 글자 (`argv[1][1]`)의 위치에서 `atoi`를 호출하여 integer로 바꿔 `JID` 값을 가져온다.
    * 해당 `JID`를 가지는 Job이 있는 지 확인하고 (없는 경우 중단), Process ID를 가져온다.
  * `isdigit(argv[1][0])` : `PID` 포맷인 경우
    * `argv[1]`에 바로 `atoi`를 적용하여 `PID` 값을 가져온다.
    * 해당 `PID`에 해당하는 Job을 검색하고 (없는 경우 중단), Job ID를 가져온다.

  Parsing part가 완료되었을 시점엔 해당 `PID` 혹은 `%JID`에 해당하는 Job의 Process ID 및 Job ID를 모두 가지고 있는 상황이 된다.

* **Job changing**

  * `argv[0]`으로 주어진 Command가 `fg`이거나 `bg`인지에 따라 Job의 `state` field를 바꾼다.
    * `job->state = FG` (`argv[0]`이 `"fg"` 인 경우)  / `job->state = BG` (`argv[0]`이 `"bg"` 인 경우) 
  * 현재 진행 중인 Job을 다시 Schedule을 걸기 위해 `kill(-process_id, SIGCONT)`를 사용해 Job에 해당하는 Process에 `SIGCONT` Signal을 보낸다. 
    * `kill`에 들어가는 `PID`가 **음수**이기 때문에, 해당 `process_id`에 해당하는 `pgroup`의 모든 Process에 대해 같은 `SIGCONT`의 Signal을 보내게 된다.
  * 이후 동작은 해당 Job의 Foreground / Background 여부에 따라,
    * Foreground인 경우 해당 Job이 끝날 때 까지 `waitfg`를 통해 Busy wait을 한다.
    * Background인 경우 Job을 추가하는 Prompt를 출력한다.

#### `waitfg`

Foreground Job이 돌고 있는 동안, Shell이 기다려 줘야 한다. 따라서 Foreground Job의 Process ID를 인자로 전달하고, 해당 job이 `fgpid(jobs)`, 즉 현재 Jobs 중 foreground에서 돌고 있는 동안 Busy wait을 하도록 구현하였다.

### Signal handlers

#### `sigchld_handler`

`SIGCHLD`는 Child Process가 `kill(process, SIGNAL)`, 혹은 Child Process의 Exit으로 인해 Child process를 Parent Process가 Reaping해야 하는 경우 Parent Process에게 전달되는 Signal이다. `tsh`에서는 Child process에 대한 `SIGINT`와 `SIGTSTP`만을 고려했기에, 해당하는 Signal의 handling만을 구현하였다.

자신의 모든 Child process의 Signal을 handling할 수 있도록 `waitpid(-1, &child_code, WNOHANG | WUNTRACED)`로 `while` loop를 순회한다. `WNOHANG` Flag가 켜진 경우 `waitpid`의 반환은 다음과 같다.

* `wait`으로 Reap된 Child process가 존재하는 경우 Child process의 `pid`를 반환한다.
* `WNOHANG` flag가 켜졌기 때문에 Reap된 Child가 없을 때 `0`을 반환한다.
* 에러 시 `-1`을 반환한다.

따라서 `while` 반복의 조건을 `process_id > 0` 인 경우로 발생할 수 있는 모든 Child process를 Reaping하도록 구현했다.

Signal Handling Part는 다음과 같이 구성하였다.

* Child Process가 `SIGINT` Signal을 받은 경우 **(Termination)**
  * `Ctrl-C`를 받았을 때 일어난다. 즉시 Termination이 일어나며, 이 경우 Parent process의 Reaping 시 전역 Job list에서 job을 삭제한다. 
* Child Process가`SIGTSTP` Signal을 받은 경우 **(Stopped)**
  * `Ctrl-Z`를 받았을 때 일어난다. Job을 Stop하며, 해당 경우 Job이 잠시 중단된다. (이후 `fg` 혹은 `bg`를 통해 Job이 재개될 수 있다.) 따라서, Job의 `state`를 `ST`로 바꾸고, Job을 삭제하지 않는다.
* Child Process가 정상 종료된 경우
  * 정상 종료된 경우 `jobs`로부터 종료된 Child process의 Job을 삭제한다.

#### `sigint_handler` 및 `sigtstp_handler`

해당하는 Signal은 항상 Foreground에서 동작하는 Job에 전달되어야 한다. 따라서 어떤 Process에서 해당 Signal Handler가 호출되더라도,  (`fgpid(jobs) <= 0`)으로  유효하지 않은 `PID`를 가지거나, 재차 `getjobpid(jobs, fgjob_id)->state != FG`로 Foreground Job이 아닌지 다시 한 번 확인하였다. 최종적 `kill` 으로 Signal을 전달할 대상은 `fgpid(-jobs)`로 Foreground Job과 해당 Job과 같은 `pgroup`의 모든 Process로 고정하였다. 

