# Micro stuttering and bad frame pacing

TODO frame this entire dev journal/article about how IIDX works across engine generations and
hardware and what the problems are. the goal is to take a very holistic approach but with a detailed
view on selected aspects of these generations. consider this my personal knowledge dump and 
understanding of all this based on the work i have done thus far (and all the stuff that came
back up in the last weeks when working again on this).

TODO merge from other branch with dev journal WIP snippets regarding ezusb stuff and the other
one that i started.

have code snippets of:
* IO processing (gameplay side where it polls it, ezusb API and how ezusb updates stuff in the background async)
* rendering
  * apitrace example trace cut down to relevant parts
  * specifically depict frame pacing
  * specifically depict monitor check
* sound processing (???)
  * depict fixed frame rate thing
  * dsound vs. wasapi/asio in later games(?)

TODO separate section to properly outline the engine generations. base the entire article on that
and outline each generation regarding hardware used/software, IO, code snippets and problems

assumed engine generations (TODO verify):
* 1-8 (SD era with twinkle hardware): Talk about this to set the ground and where everything was
  coming from. explain how the hardware operated, the realtime requirements and how the hardware met
  them. This also likely set the understanding of the devs and the expectations for the game
  engine and how everything behaves and works software-wise, but coupled very tightly to the hardware
  requirements (which is one of the key root causes for all the mess that came after that with the
  PC platform)
* 9-13 (SD era with ezusb1)
* 14 - 19 (SD era with ezusb1/2, monitor check added)
* 20 - 24 (HD era with ezusb1/2, auto monitor check added)
* 25 - 26 (non lightning but bio2)
* 27 - 29 (two cabint types, bia2x added, 120 fps mode, two different hardware platforms for each cabinet type)
* 30 (FHD mode)

## iidx 24

* frame pacing before present... D:
* always sleeps at least 2 ms, even if behind frame time
* only sleep if frame time is less than 14 ms, if longer, just don't frame pace

```c
int renderAndFramePace()
{
  DWORD frame_time_ms; // eax
  DWORD sleep_ms; // ecx
  IDirect3DDevice9 *v2; // eax
  IDirect3DDevice9 *v3; // eax

  frame_time_ms = timeGetTime() - previousFrameTime;
  if ( frame_time_ms < 14 )
  {
    sleep_ms = 14 - frame_time_ms;
    if ( 14 - frame_time_ms < 2 )
      sleep_ms = 2;
    SleepEx(sleep_ms, 0);
  }
  v2 = getD3D9Device();
  if ( v2->lpVtbl->Present(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = getD3D9Device();
    if ( v3->lpVtbl->TestCooperativeLevel(v3) == -2005530519 )
      sub_100A46F0();
  }
  previousFrameTime = timeGetTime();
  return 0;
}
```

## 10th style

### Windows API usage and references

The engine uses the following Windows API functions to implement timing and
frame pacing related logic. Included excerpts and information relevant for
further discussion in this conext.

* [timeBeginPeriod](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod)
  * `MMRESULT timeBeginPeriod(UINT uPeriod);`
  * Set the minimum timer resolution in **milliseconds** for the application
* [timeGetTime](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timegettime)
  * `DWORD timeGetTime();`
  * Returns system time in **milliseconds**
* [Sleep](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep)
  * `void Sleep([in] DWORD dwMilliseconds);`
  * The time interval for which execution is to be suspended, in **milliseconds**.
  * A value of zero causes the thread to relinquish the remainder of its time slice to any other
    thread that is ready to run.
  * If there are no other threads ready to run, the function returns immediately, and the thread
    continues execution.
  * **Windows XP**: A value of zero causes the thread to relinquish the remainder of its time slice
    to any other thread of equal priority that is ready to run. If there are no other threads of
    equal priority ready to run, the function returns immediately, and the thread continues
    execution. **This behavior changed starting with Windows Server 2003.**

### Windows API analysis and impact

* Only milliseconds accuracy
  * That's not a problem ultimately but requires attention that you obviously cannot
    get more accurate with using only these API calls in your logic
* Different behavior on different versions of Windows
  * Not a problem back then, but became a problem for home users and also Konami once
    they moved away from XP without updating the code. The code's behavior is highly
    depending on the OS's underlying behavior of scheduling threads
* Threading and scheduling by the OS - Realtime requirements on a non realtime OS
  * I would argue this being one of if not even the root cause for why all games
    starting on a PC hardware platform with Windows OSes started to have significant
    problems
  * Previously, the game ran on truely embedded hardware which was capable of
    fulfilling realtime requirements. These boil down to a (sort of) deadline
    guarantee for the game update/render loop with a stable and known delay
    = constant/stable FPS. Therefore, the entire game engine could be built
    on top of that. 
    This is where the game's engine roots in for basing all their logic on frames
    and not time. The twinkle hardware's most reliable time source was on a 
    frame basis. There was no clock/time/CPU counter with decent enough accuracy
    available at that time, likely.
    Then, when the devs decided to switch to the PC platform, this is where some
    hardware problems at that time and era of PC hardware came up: Frame times
    were not guaranteed to be predictiable because Win XP is not a realtime OS
    and cannot provide realtime guarantees for software. The game's software has
    to compete for computing time with the OS interrupting it to also do other
    stuff. Sound hardware has more abstraction layers which add additional
    latency to sound processing. IO is more complicated to drive and manage
    due to indirect communication with a complex bus system (USB).
    The game had one solid source of time, the constant frame
    rate, that was guaranteed by hardware and could not be interrupted and
    interfered by software. The latter applies because the game ran on a minimal
    OS/BIOS only. Naturally, this hardware was hard to develop for, but was
    a perfect match requirement-wise for what one has to achieve with a solid
    game play expierence: Smooth 60 FPS, non stuttering game update loops creating
    a smooth game play experience with solid gameplay timing and everything
    syncs up perfectly (picture on monitor with music and IO).
    The devs tried to solve all these problems with the following tools but
    only succeeded partially:
    * Use multi-threading to interleave processing of different parts of the
      game logic, rendering, IO and sound (???), to increase resource 
      utilization to meet the 60 FPS deadline.
      Note that this was also a problem that the hardware at the time was not
      actual multi core hardware, P4 era of CPUs. Therefore, the devs were
      never able to test their code properly and never ran into data and timing
      races due to poor concurrency management and synchronization
    * Enforce "realtime like" scheduling with various means like thread
      and process prioritization. Run the process with the realtime flags for
      windows and assign different priorities to different threads
    * Frame pacing the game update loop to create a stable and smooth 60 FPS
      game play experience. The key was to sync up, display output, sound output
      and IO with each frame consistently and on rock solid constant frame times
      every frame. Otherwise, you get all of these symptoms: Choppy framerates,
      microstutter, laggy/stuttering IO, audio to screen desync
* How did this evolve over the years? Different hardware and software eras duct
  taped the problem in different ways to this day
  * Gen 1: 9 to 13
    * D3D8 rendering API
    * Windows XP based
    * P4 hardware, single core (TODO arcade doc reference)
    * Officially supported monitors: Analog VGA connected CRT display, only two types officially supported. The game's
      hardcoded to assume it can always run on 59.95 hz on these monitors.
    * IO hardware ezusb with very flaky ezusb code -> lots of sleeps in the code because they could
      not figure out the right timing for this. the impact on this becomes more apparent once
      the hardware platform moves into multi core -> flaky USBIO timeouts more frequent
    * 9th style started off as the MVP for the PC platform. clear evidence for this are the many
      different revisions and the game was known to be very buggy, especially in the first revisions
      (TODO ref bemaniwiki). It gotten better throughout the revisions but the life span of this
      version was rather short (TODO how long?) and 10th style followed only X months after.
      apparently, konami wanted to make up for this to owners by giving owners of 9th style a free
      software upgrade to 10th style, which consisted of only the HDD, as the code supports the
      C02 IO and a C02 upgrade dongle with the product code `GEC02JA` (TODO code snippet of
      security section showing that C02 and D01 dongles work). Aside that, konami also released
      a new dedicated cabinet with D01 and the ezusb 1 IO received an upgrade, the so called "D01"
      IO. The main differences to the "C02" IO are that the hardware is wired up differently. It
      does not support the old style twinkle IO subboard anymore, connectors are different. One
      key difference is that the turntables are now hooked up directly to the IO's FPGA instead of
      going via a serial connection via the sub-IO board behind the effector panel which reduced
      latency for the turntable input significantly and improved gameplay experience. this IO
      hardware/setup was used throughout 10 to 13.
  * Gen 2: 14 to 19
    * D3D9 API
    * Windows XP based
    * TODO arcade docs hardware
    * Officially supported monitors: SD and HD throughout the versions. Game engine now supports switching between
      either 59.95 ("SVIDEO") monitor refresh rate or 60.05 ("VGA"). This started with the well known "monitor check"
      screen that appeared first on 14. it measures the frame time of the render loop and picks the refresh rate
      closes to the measured render time. The logic only takes a sample size of one and uses the last frame as the
      result to base the decision on (TODO needs evidence in code). TODO what operator menu options do i have on gold
      here?
      With 15, they introduced a new LCD type monitor which also supports DVI next to VGA. This added new challenges
      to the mix as the digital connection created a different resulting frame rate for the game than the analog
      connection. I would argue that the devs became more aware of the difficulties of syncing up with different
      hardware at this point.
      Nothing changed throughout the versions 16 to 17 regarding operator menu settings or hardwardware. 18 got
      another hardware upgrade (TODO arcade docs ref). Again, the new hardware impacted the gameplay experience
      as it changed the behavior how their software ran on it and how the timing played out with the different
      variables involved: monitor refresh rate on different monitors and types of connections (digital (DVI) vs.
      analog (SVIDEO,VGA)), CPU now actual multi core (???), though still windows XP
      With the very last revision of iidx 19, the devs indicated that they could not continue with the very
      static engine timing setup of assuming there is only a known set of "refresh rates" the game runs on. this
      is the first time the "monitor check with FPS" appeared. however, this implementation (TODO code reference)
      does only measure the framerate and write it to a file on disk for analysis by the dev. i assume they wanted
      to collect data to understand how diverse the hardware mix is at this point. the game still operates like
      14 to 18 regarding the way the refresh rate is determined and configured for the game engine
  * Gen 3: 20 to 26
     * D3D9 API
     * Windows XP (???) when win 7?
     * Intro of 64-bit when?
     * Hardware 20 to 24
     * Hardware 25 to 26
     * IO hardware switch to BIO2 -> getting rid of bad ezusb code with all those sleeps
  * Gen 4: 27 to 30
    * D3D9Ex API
    * Hardware split due to new lightning cabinet
    * Windows 7 64 bit only
    * 60 hz screens vs. 120 hz screens
    * Old cabs still running ADE hardware (arcade docs ref TODO) but lightning cab (TDJ) having
      own hardware since 27 (TODO arcade docs)
    * New monitor check code and frame pacing (???) to address 120 hz concerns
    * The devs are struggling a lot with many bugs on lightning cabinets


### Code from game

```c
// sub_4790D0
int __thiscall frame_pace_outer(_DWORD *this, int a2)
{
  DWORD v3; // ecx
  int v4; // eax
  int v5; // ecx

  v3 = 625 * (timeGetTime() - this[63]);
  v4 = this[61];
  v5 = 16 * v3;
  if ( v5 < 2 * v4 || v5 < 2 * this[62] )
    this[61] = (v5 + 2 * v4 + v4) / 4;
  this[62] = v5;
  return frame_pace_maybe(this);
}
```

```c
// sub_479030
void __thiscall frame_pace_maybe(_DWORD *this)
{
  int v1; // ecx

  v1 = this[60];
  if ( v1 <= 0 )
    Sleep(0);
  else
    Sleep(v1 / 10000);
}
```

### Threading primitives used from win32 api

* [CreateThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread)
* [SetThreadPriority](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority)
  * Priorities
    * `THREAD_PRIORITY_BELOW_NORMAL`: `-1`

### Analysis threads used on 10th style engine

* process: main thread via WinMain
* long-running threads, life cycle throughout entire application, threads spawned via `CreateThread` API
  * network thread for eamuse, thread proc `sub_465AA0`
  * something during boot to speed up startup, i assume ??? `sub_473D40`
    * Sets its own priority to `-1`
  * `sub_475BC0`: Some thread to apparently load files with unbuffered IO, several [mmio](https://learn.microsoft.com/en-us/previous-versions/dd757331(v=vs.85)) calls throughout the function
  * ezusb: TODO
* short-running threads, spawned and used for a brief task, then killed
  * threads spawned via `CreateThread` API via a wrapper function `sub_487550`, thread priority set to `-1` with `SetThreadPriority`

### DWORD __stdcall sub_47C6E0(int a1, DWORD dwMilliseconds, HWND hWnd, UINT wMsgFilterMin, DWORD nCount)

This is a very interesting one. it seems like this piece of logic is trying to influence thread scheduling on the system
based on how long it took the thread to do something. the `> 10 ms` part seems to indicate that this is applied to
the main thread. so if the main thread is too slow, the priority of it is boosted

this logic apparently switches the main thread priority around if necessary

## summary frame pacing code

### None at all?

* 09
* 10

### V1 with while loop

11-13

```c
//timeBeginPeriod(1u);
  while ( (double)timeGetTime() - prev_frame_time_ms < 13.0 )
  {
    Sleep(1u);
    //timeBeginPeriod(1u);
  }
```

### V2 while loop with SleepEx

14

```c
while ( timeGetTime() - dword_2943E10 < 0xD )
    SleepEx(1u, 0);
```

### V3

15-26

```c
frame_time_ms = timeGetTime() - previousFrameTime;
  if ( frame_time_ms < 14 )
  {
    sleep_ms = 14 - frame_time_ms;
    if ( 14 - frame_time_ms < 2 )
      sleep_ms = 2;
    SleepEx(sleep_ms, 0);
  }
```

### V4

27-29

```c
QueryPerformanceCounter(&current_frame_time_counter);
  v5 = get_monitor_check_framerate_maybe();
  target_frame_rate = get_target_frame_rate(v5);
  time_to_sleep = 1000.0 / (double)target_frame_rate
                - (double)(current_frame_time_counter.LowPart - (int)previous_frame_time_counter)
                * 1000.0
                / (double)(int)Frequency.LowPart
                - 3.0;                          // _W_T_F_?!
  if ( time_to_sleep > 0.0 )
    SleepEx((int)time_to_sleep, 0);             // only MS accuracy
```

## iidx 09 JAG

no frame pace code at all?

the stuff below doesn't contain the present call but does some time tracking per frame, apparently

```c
DWORD __thiscall sub_428950(_DWORD *this)
{
  DWORD result; // eax
  unsigned int v3; // ecx
  int v4; // edx
  unsigned int v5; // ecx

  result = timeGetTime();
  v3 = this[1] + (unsigned __int16)this[2];
  v4 = this[3];
  this[2] = v3;
  v5 = HIWORD(v3);
  if ( result - v4 < v5 )
  {
    this[3] = v5 + v4;
    result = timeGetTime();
    this[(this[14] & 0x1F) + 115] = result;
  }
  else
  {
    this[3] = result;
  }
  return result;
}
```

## iidx 10 JAE

no frame pace code at all?

```c
int render_frame()
{
  int v0; // eax
  int *v1; // esi
  int v2; // edi
  int v3; // eax

  v0 = get_d3d_device();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v0 + 60))(v0, 0, 0, 0, 0) == -2005530520 )// D3D Present
  {
    v1 = (int *)get_d3d_device();
    v2 = *v1;
    v3 = sub_403E60();
    (*(void (__stdcall **)(int *, int))(v2 + 56))(v1, v3);
  }
  return 0;
}
```

## iidx 11 JAA

```c
// 0043F3E0
int render_frame_and_pace()
{
  int v0; // eax
  _DWORD *v1; // eax
  int v2; // ebx
  int v3; // eax
  double now; // st7
  int result; // eax
  _DWORD *v6; // [esp+1Ch] [ebp-8h]

  timeBeginPeriod(1u);
  while ( (double)timeGetTime() - prev_frame_time_ms < 13.0 )
  {
    Sleep(1u);
    timeBeginPeriod(1u);
  }
  v0 = get_d3d_device();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v0 + 60))(v0, 0, 0, 0, 0) == -2005530520 )// Present
  {
    v1 = (_DWORD *)get_d3d_device();
    v2 = *v1;
    v6 = v1;
    v3 = sub_403C30();
    (*(void (__stdcall **)(_DWORD *, int))(v2 + 56))(v6, v3);
  }
  timeBeginPeriod(1u);
  now = (double)timeGetTime();
  result = 0;
  prev_frame_time_ms = now;
  return result;
}
```

## iidx 12 JAD

```c
int sub_44B530()
{
  int v0; // eax
  int *v1; // edi
  int v2; // ebx
  int v3; // eax

  while ( timeGetTime() - dword_1978BF8 < 0xD )
    Sleep(1u);
  v0 = sub_43E950();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v0 + 60))(v0, 0, 0, 0, 0) == -2005530520 )
  {
    v1 = (int *)sub_43E950();
    v2 = *v1;
    v3 = sub_43E960();
    (*(void (__stdcall **)(int *, int))(v2 + 56))(v1, v3);
  }
  dword_1978BF8 = timeGetTime();
  return 0;
}
```

## iidx 13 JAG

```c
int sub_473A80()
{
  int v0; // eax
  int *v1; // edi
  int v2; // ebx
  int v3; // eax

  while ( timeGetTime() - dword_1FF967C < 0xD )
    Sleep(1u);
  v0 = sub_4028C9();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v0 + 60))(v0, 0, 0, 0, 0) == -2005530520 )
  {
    v1 = (int *)sub_4028C9();
    v2 = *v1;
    v3 = sub_401712();
    (*(void (__stdcall **)(int *, int))(v2 + 56))(v1, v3);
  }
  dword_1FF967C = timeGetTime();
  return 0;
}
```

## iidx 14 2007072301

```c
int sub_476940()
{
  int v0; // eax
  int *v1; // edi
  int v2; // ebx
  int v3; // eax

  while ( timeGetTime() - dword_2943E10 < 0xD )
    SleepEx(1u, 0);
  v0 = sub_4014A1();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v0 + 68))(v0, 0, 0, 0, 0) == -2005530520 )
  {
    v1 = (int *)sub_4014A1();
    v2 = *v1;
    v3 = sub_4019AB();
    (*(void (__stdcall **)(int *, int))(v2 + 64))(v1, v3);
  }
  dword_2943E10 = timeGetTime();
  return 0;
}
```

## iidx 15 2008031100

```c
int sub_4812B0()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  int *v3; // esi
  int v4; // ebx
  int v5; // eax

  v0 = timeGetTime() - dword_29F5030;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_401500();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = (int *)sub_401500();
    v4 = *v3;
    v5 = sub_401AB9();
    (*(void (__stdcall **)(int *, int))(v4 + 64))(v3, v5);
  }
  dword_29F5030 = timeGetTime();
  return 0;
}
```

## iidx 16 2009072200

```c
int sub_48E1A0()
{
  int (*v0)(void); // edi
  DWORD v1; // eax
  DWORD v2; // ecx
  int v3; // eax
  int *v4; // esi
  int v5; // ebx
  int v6; // eax

  v0 = *(int (**)(void))timeGetTime;
  v1 = timeGetTime() - dword_2981B28;
  if ( v1 < 0xE )
  {
    v2 = 14 - v1;
    if ( 14 - v1 < 2 )
      v2 = 2;
    SleepEx(v2, 0);
  }
  v3 = sub_401591();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v3 + 68))(v3, 0, 0, 0, 0) == -2005530520 )
  {
    v4 = (int *)sub_401591();
    v5 = *v4;
    v6 = sub_401BD6();
    (*(void (__stdcall **)(int *, int))(v5 + 64))(v4, v6);
  }
  dword_2981B28 = v0();
  return 0;
}
```

## iidx 17 2010071200

```c
int sub_42F3D0()
{
  int (*v0)(void); // ebx
  DWORD v1; // eax
  DWORD v2; // ecx
  int v3; // eax
  _DWORD *v4; // edi
  void (__stdcall **v5)(_DWORD *, int); // esi
  int v6; // eax

  v0 = *(int (**)(void))timeGetTime;
  v1 = timeGetTime() - dword_239C828;
  if ( v1 < 0xE )
  {
    v2 = 14 - v1;
    if ( 14 - v1 < 2 )
      v2 = 2;
    SleepEx(v2, 0);
  }
  v3 = sub_424340();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v3 + 68))(v3, 0, 0, 0, 0) == -2005530520 )
  {
    v4 = (_DWORD *)sub_424340();
    v5 = (void (__stdcall **)(_DWORD *, int))(*v4 + 64);
    v6 = sub_424350();
    (*v5)(v4, v6);
  }
  dword_239C828 = v0();
  return 0;
}
```

## iidx 18 2011071200

```c
int sub_100315D0()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  _DWORD *v3; // edi
  void (__stdcall **v4)(_DWORD *, int); // esi
  int v5; // eax

  v0 = timeGetTime() - dword_104645B0;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_100278C0();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = (_DWORD *)sub_100278C0();
    v4 = (void (__stdcall **)(_DWORD *, int))(*v3 + 64);
    v5 = sub_100278D0();
    (*v4)(v3, v5);
  }
  dword_104645B0 = timeGetTime();
  return 0;
}
```

## iidx 19 2012090300

```c
int sub_10076BE0()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  _DWORD *v3; // edi
  void (__stdcall **v4)(_DWORD *, int); // esi
  int v5; // eax

  v0 = timeGetTime() - dword_10545138;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_1007D010();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = (_DWORD *)sub_1007D010();
    v4 = (void (__stdcall **)(_DWORD *, int))(*v3 + 64);
    v5 = sub_1007D020();
    (*v4)(v3, v5);
  }
  dword_10545138 = timeGetTime();
  return 0;
}
```

## iidx 20 2013090900

```c
int sub_1008ACD0()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  int v3; // eax

  v0 = timeGetTime() - dword_10C7F398;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_10092690();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = sub_10092690();
    if ( (*(int (__stdcall **)(int))(*(_DWORD *)v3 + 12))(v3) == -2005530519 )
      sub_100929B0();
  }
  dword_10C7F398 = timeGetTime();
  return 0;
}
```

## iidx 21 2014071600

```c
int sub_10091C10()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  int v3; // eax

  v0 = timeGetTime() - dword_1141DA98;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_10099D50();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = sub_10099D50();
    if ( (*(int (__stdcall **)(int))(*(_DWORD *)v3 + 12))(v3) == -2005530519 )
      sub_1009A070();
  }
  dword_1141DA98 = timeGetTime();
  return 0;
}
```

## iidx 22 2015080500

```c
int sub_1009CC40()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  int v3; // eax

  v0 = timeGetTime() - dword_1143D64C;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_100A5690();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = sub_100A5690();
    if ( (*(int (__stdcall **)(int))(*(_DWORD *)v3 + 12))(v3) == -2005530519 )
      sub_100A59B0();
  }
  dword_1143D64C = timeGetTime();
  return 0;
}
```

## iidx 23 2016083100

```c
int sub_10090080()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  int v2; // eax
  int v3; // eax

  v0 = timeGetTime() - dword_1142497C;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_10098220();
  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)v2 + 68))(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = sub_10098220();
    if ( (*(int (__stdcall **)(int))(*(_DWORD *)v3 + 12))(v3) == -2005530519 )
      sub_10098520();
  }
  dword_1142497C = timeGetTime();
  return 0;
}
```

## iidx 24 2017082800

```c
int renderAndFramePace()
{
  DWORD frame_time_ms; // eax
  DWORD sleep_ms; // ecx
  IDirect3DDevice9 *v2; // eax
  IDirect3DDevice9 *v3; // eax

  frame_time_ms = timeGetTime() - previousFrameTime;
  if ( frame_time_ms < 14 )
  {
    sleep_ms = 14 - frame_time_ms;
    if ( 14 - frame_time_ms < 2 )
      sleep_ms = 2;
    SleepEx(sleep_ms, 0);
  }
  v2 = getD3D9Device();
  if ( v2->lpVtbl->Present(v2, 0, 0, 0, 0) == -2005530520 )
  {
    v3 = getD3D9Device();
    if ( v3->lpVtbl->TestCooperativeLevel(v3) == -2005530519 )
      sub_100A46F0();
  }
  previousFrameTime = timeGetTime();
  return 0;
}
```

## iidx 25 2018091900

```c
__int64 sub_18012D850()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  __int64 v2; // rax
  __int64 v3; // rax

  v0 = timeGetTime() - dword_1824AF614;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_18010AB20();
  if ( (*(unsigned int (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD))(*(_QWORD *)v2 + 136i64))(
         v2,
         0i64,
         0i64,
         0i64,
         0i64) == -2005530520 )
  {
    v3 = sub_18010AB20();
    if ( (*(unsigned int (__fastcall **)(__int64))(*(_QWORD *)v3 + 24i64))(v3) == -2005530519 )
      sub_18010A8D0();
  }
  dword_1824AF614 = timeGetTime();
  return 0i64;
}
```

## iidx 26 2019090200

```c
__int64 sub_1803928F0()
{
  DWORD v0; // eax
  DWORD v1; // ecx
  __int64 v2; // rax
  __int64 v3; // rax

  v0 = timeGetTime() - dword_183146104;
  if ( v0 < 0xE )
  {
    v1 = 14 - v0;
    if ( 14 - v0 < 2 )
      v1 = 2;
    SleepEx(v1, 0);
  }
  v2 = sub_18036D9F0();
  if ( (*(unsigned int (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD))(*(_QWORD *)v2 + 136i64))(
         v2,
         0i64,
         0i64,
         0i64,
         0i64) == -2005530520 )
  {
    v3 = sub_18036D9F0();
    if ( (*(unsigned int (__fastcall **)(__int64))(*(_QWORD *)v3 + 24i64))(v3) == -2005530519 )
      sub_18036D7A0();
  }
  dword_183146104 = timeGetTime();
  return 0i64;
}
```

## iidx 27 2020092900

```c
__int64 sub_1805CCEE0()
{
  int v0; // er15
  __int64 v1; // rbx
  __int64 v2; // rdi
  __int64 v3; // rsi
  __int64 v4; // rax
  int v5; // eax
  double v6; // xmm3_8
  int v7; // eax
  unsigned int v8; // er14
  unsigned int v9; // ecx
  int v10; // eax
  int v11; // er11
  __int64 v13; // [rsp+0h] [rbp-79h] BYREF
  LARGE_INTEGER PerformanceCount; // [rsp+30h] [rbp-49h] BYREF
  __int64 v15; // [rsp+38h] [rbp-41h] BYREF
  LARGE_INTEGER v16; // [rsp+40h] [rbp-39h] BYREF
  LARGE_INTEGER v17; // [rsp+48h] [rbp-31h] BYREF
  LARGE_INTEGER v18; // [rsp+50h] [rbp-29h] BYREF
  __int64 v19; // [rsp+58h] [rbp-21h]
  __int64 v20; // [rsp+60h] [rbp-19h]
  __int64 v21; // [rsp+68h] [rbp-11h]
  __int128 v22; // [rsp+70h] [rbp-9h] BYREF
  __int128 v23; // [rsp+80h] [rbp+7h]
  __int128 v24; // [rsp+90h] [rbp+17h] BYREF
  __int128 v25; // [rsp+A0h] [rbp+27h]
  __int64 v26; // [rsp+B0h] [rbp+37h]

  v19 = -2i64;
  v0 = 0;
  v1 = sub_180595510();
  v20 = *(_QWORD *)sub_180595540();
  v2 = v20;
  if ( v20 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v20 + 8i64))(v20);
  v21 = *(_QWORD *)sub_180595550();
  v3 = v21;
  if ( v21 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v21 + 8i64))(v21);
  QueryPerformanceCounter(&PerformanceCount);
  v4 = sub_1805952D0();
  v5 = sub_180621D60(v4);
  v6 = 1000.0 / (double)v5
     - (double)(PerformanceCount.LowPart - (int)qword_1847CE230) * 1000.0 / (double)(int)qword_1847CE218
     - 3.0;
  if ( v6 > 0.0 )
    SleepEx((int)v6, 0);
  QueryPerformanceCounter(&v16);
  if ( v2 )
  {
    v7 = (*(__int64 (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD, _DWORD))(*(_QWORD *)v2 + 24i64))(
           v2,
           0i64,
           0i64,
           0i64,
           0i64,
           0);
    v8 = v7;
    if ( v7 <= 0 )
    {
      if ( !v7 )
        goto LABEL_16;
      v9 = v7 + 2005530520;
      if ( (unsigned int)(v7 + 2005530520) <= 0xC )
      {
        v10 = 4353;
        if ( _bittest(&v10, v9) )
          goto LABEL_16;
      }
LABEL_15:
      avs2_core_380("Vsync", "PresentEx fail hr=0x%08X", v8);
      goto LABEL_16;
    }
    if ( v7 != 141953143 && v7 != 141953144 )
      goto LABEL_15;
  }
LABEL_16:
  QueryPerformanceCounter(&v17);
  v15 = 0i64;
  if ( v3 && (*(int (__fastcall **)(__int64, __int64, __int64 *))(*(_QWORD *)v1 + 152i64))(v1, 1i64, &v15) >= 0 )
    (*(__int64 (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD, _DWORD))(*(_QWORD *)v3 + 24i64))(
      v3,
      0i64,
      0i64,
      0i64,
      0i64,
      0);
  QueryPerformanceCounter(&v18);
  v22 = 0ui64;
  v23 = 0ui64;
  v24 = 0ui64;
  v25 = 0ui64;
  if ( v2 )
    (*(void (__fastcall **)(__int64, __int128 *))(*(_QWORD *)v2 + 88i64))(v2, &v22);
  if ( v3 )
    (*(void (__fastcall **)(__int64, __int128 *))(*(_QWORD *)v3 + 88i64))(v3, &v24);
  v11 = 0;
  if ( qword_1847CE210 )
    v11 = DWORD1(v22) - DWORD1(xmmword_1847CE280) - (v22 - xmmword_1847CE280);
  if ( qword_1847CE210 )
    v0 = DWORD1(v24) - DWORD1(xmmword_1847CE2A0) - (v24 - xmmword_1847CE2A0);
  *((double *)&xmmword_1847CE2C0 + 1) = (double)(PerformanceCount.LowPart - (int)qword_1847CE238)
                                      / (double)(int)qword_1847CE218;
  *(double *)&xmmword_1847CE2D0 = (double)(v16.LowPart - PerformanceCount.LowPart) / (double)(int)qword_1847CE218;
  *((double *)&xmmword_1847CE2D0 + 1) = (double)(v17.LowPart - v16.LowPart) / (double)(int)qword_1847CE218;
  *(double *)&xmmword_1847CE2E0 = (double)(v18.LowPart - (int)qword_1847CE238) / (double)(int)qword_1847CE218;
  *(_QWORD *)&xmmword_1847CE2C0 = qword_1847CE210;
  *(__int128 *)((char *)&xmmword_1847CE2E0 + 8) = v22;
  *(__int128 *)((char *)&xmmword_1847CE2F0 + 8) = v23;
  dword_1847CE308 = v11;
  *(_QWORD *)&xmmword_1847CE310 = v11 + (_QWORD)xmmword_1847CE310;
  *((double *)&xmmword_1847CE310 + 1) = (double)(v18.LowPart - v17.LowPart) / (double)(int)qword_1847CE218;
  xmmword_1847CE320 = v24;
  xmmword_1847CE330 = v25;
  LODWORD(xmmword_1847CE340) = v0;
  *((_QWORD *)&xmmword_1847CE340 + 1) += v0;
  qword_1847CE350 = v15;
  qword_1847CE220 = PerformanceCount.QuadPart;
  qword_1847CE228 = v16.QuadPart;
  qword_1847CE230 = v17.QuadPart;
  qword_1847CE238 = v18.QuadPart;
  xmmword_1847CE280 = v22;
  xmmword_1847CE290 = v23;
  xmmword_1847CE2A0 = v24;
  xmmword_1847CE2B0 = v25;
  ++qword_1847CE210;
  if ( v3 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v3 + 16i64))(v3);
  if ( v2 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v2 + 16i64))(v2);
  return sub_1806D3080((unsigned __int64)&v13 ^ v26);
}
```

## iidx 28 2021091500

```c
__int64 sub_180789C80()
{
  int v0; // er15
  __int64 v1; // rbx
  __int64 v2; // rdi
  __int64 v3; // rsi
  __int64 v4; // rax
  int v5; // eax
  double v6; // xmm3_8
  int v7; // eax
  unsigned int v8; // er14
  unsigned int v9; // ecx
  int v10; // eax
  int v11; // er11
  __int64 v13; // [rsp+0h] [rbp-79h] BYREF
  LARGE_INTEGER PerformanceCount; // [rsp+30h] [rbp-49h] BYREF
  __int64 v15; // [rsp+38h] [rbp-41h] BYREF
  LARGE_INTEGER v16; // [rsp+40h] [rbp-39h] BYREF
  LARGE_INTEGER v17; // [rsp+48h] [rbp-31h] BYREF
  LARGE_INTEGER v18; // [rsp+50h] [rbp-29h] BYREF
  __int64 v19; // [rsp+58h] [rbp-21h]
  __int64 v20; // [rsp+60h] [rbp-19h]
  __int64 v21; // [rsp+68h] [rbp-11h]
  __int128 v22; // [rsp+70h] [rbp-9h] BYREF
  __int128 v23; // [rsp+80h] [rbp+7h]
  __int128 v24; // [rsp+90h] [rbp+17h] BYREF
  __int128 v25; // [rsp+A0h] [rbp+27h]
  __int64 v26; // [rsp+B0h] [rbp+37h]

  v19 = -2i64;
  v0 = 0;
  v1 = sub_18074F630();
  v20 = *(_QWORD *)sub_18074F660();
  v2 = v20;
  if ( v20 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v20 + 8i64))(v20);
  v21 = *(_QWORD *)sub_18074F670();
  v3 = v21;
  if ( v21 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v21 + 8i64))(v21);
  QueryPerformanceCounter(&PerformanceCount);
  v4 = sub_18074F3F0();
  v5 = ((__int64 (__fastcall *)(__int64))sub_180855810)(v4);
  v6 = 1000.0 / (double)v5
     - (double)(PerformanceCount.LowPart - (int)qword_185553A60) * 1000.0 / (double)(int)qword_185553A48
     - 3.0;
  if ( v6 > 0.0 )
    SleepEx((int)v6, 0);
  QueryPerformanceCounter(&v16);
  if ( v2 )
  {
    v7 = (*(__int64 (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD, _DWORD))(*(_QWORD *)v2 + 24i64))(
           v2,
           0i64,
           0i64,
           0i64,
           0i64,
           0);
    v8 = v7;
    if ( v7 <= 0 )
    {
      if ( !v7 )
        goto LABEL_16;
      v9 = v7 + 2005530520;
      if ( (unsigned int)(v7 + 2005530520) <= 0xC )
      {
        v10 = 4353;
        if ( _bittest(&v10, v9) )
          goto LABEL_16;
      }
LABEL_15:
      avs2_core_380("Vsync", "PresentEx fail hr=0x%08X", v8);
      goto LABEL_16;
    }
    if ( v7 != 141953143 && v7 != 141953144 )
      goto LABEL_15;
  }
LABEL_16:
  QueryPerformanceCounter(&v17);
  v15 = 0i64;
  if ( v3 && (*(int (__fastcall **)(__int64, __int64, __int64 *))(*(_QWORD *)v1 + 152i64))(v1, 1i64, &v15) >= 0 )
    (*(__int64 (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD, _DWORD))(*(_QWORD *)v3 + 24i64))(
      v3,
      0i64,
      0i64,
      0i64,
      0i64,
      0);
  QueryPerformanceCounter(&v18);
  v22 = 0ui64;
  v23 = 0ui64;
  v24 = 0ui64;
  v25 = 0ui64;
  if ( v2 )
    (*(void (__fastcall **)(__int64, __int128 *))(*(_QWORD *)v2 + 88i64))(v2, &v22);
  if ( v3 )
    (*(void (__fastcall **)(__int64, __int128 *))(*(_QWORD *)v3 + 88i64))(v3, &v24);
  v11 = 0;
  if ( qword_185553A40 )
    v11 = DWORD1(v22) - DWORD1(xmmword_185553AB0) - (v22 - xmmword_185553AB0);
  if ( qword_185553A40 )
    v0 = DWORD1(v24) - DWORD1(xmmword_185553AD0) - (v24 - xmmword_185553AD0);
  *((double *)&xmmword_185553AF0 + 1) = (double)(PerformanceCount.LowPart - (int)qword_185553A68)
                                      / (double)(int)qword_185553A48;
  *(double *)&xmmword_185553B00 = (double)(v16.LowPart - PerformanceCount.LowPart) / (double)(int)qword_185553A48;
  *((double *)&xmmword_185553B00 + 1) = (double)(v17.LowPart - v16.LowPart) / (double)(int)qword_185553A48;
  *(double *)&xmmword_185553B10 = (double)(v18.LowPart - (int)qword_185553A68) / (double)(int)qword_185553A48;
  *(_QWORD *)&xmmword_185553AF0 = qword_185553A40;
  *(__int128 *)((char *)&xmmword_185553B10 + 8) = v22;
  *(__int128 *)((char *)&xmmword_185553B20 + 8) = v23;
  dword_185553B38 = v11;
  *(_QWORD *)&xmmword_185553B40 = v11 + (_QWORD)xmmword_185553B40;
  *((double *)&xmmword_185553B40 + 1) = (double)(v18.LowPart - v17.LowPart) / (double)(int)qword_185553A48;
  xmmword_185553B50 = v24;
  xmmword_185553B60 = v25;
  LODWORD(xmmword_185553B70) = v0;
  *((_QWORD *)&xmmword_185553B70 + 1) += v0;
  qword_185553B80 = v15;
  qword_185553A50 = PerformanceCount.QuadPart;
  qword_185553A58 = v16.QuadPart;
  qword_185553A60 = v17.QuadPart;
  qword_185553A68 = v18.QuadPart;
  xmmword_185553AB0 = v22;
  xmmword_185553AC0 = v23;
  xmmword_185553AD0 = v24;
  xmmword_185553AE0 = v25;
  ++qword_185553A40;
  if ( v3 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v3 + 16i64))(v3);
  if ( v2 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v2 + 16i64))(v2);
  return ((__int64 (__fastcall *)(unsigned __int64))unk_180919A00)((unsigned __int64)&v13 ^ v26);
}
```

## iidx 29 2022082400

```c
__int64 render_update_and_frame_pace()
{
  int v0; // er15
  int v1; // er14
  IDirect3DDevice9Ex *d3d_device_sub_screen; // rbx
  IDirect3DDevice9Ex *d3d_device_main_screen; // rdi
  __int64 v4; // rsi
  __int64 v5; // rax
  int target_frame_rate; // eax
  double time_to_sleep; // xmm3_8
  int res_present_ex; // eax
  unsigned int v9; // ecx
  int v10; // eax
  int v11; // er11
  LARGE_INTEGER current_frame_time_counter; // [rsp+38h] [rbp-49h] BYREF
  __int64 v14; // [rsp+40h] [rbp-41h] BYREF
  LARGE_INTEGER v15; // [rsp+48h] [rbp-39h] BYREF
  LARGE_INTEGER v16; // [rsp+50h] [rbp-31h] BYREF
  LARGE_INTEGER v17; // [rsp+58h] [rbp-29h] BYREF
  __int64 v18; // [rsp+60h] [rbp-21h]
  IDirect3DDevice9Ex *v19; // [rsp+68h] [rbp-19h]
  __int64 v20; // [rsp+70h] [rbp-11h]
  __int128 v21; // [rsp+78h] [rbp-9h] BYREF
  __int128 v22; // [rsp+88h] [rbp+7h]
  __int128 v23; // [rsp+98h] [rbp+17h] BYREF
  __int128 v24; // [rsp+A8h] [rbp+27h]

  v18 = -2i64;
  v0 = 0;
  v1 = 0;
  d3d_device_sub_screen = get_d3d_device_sub_screen();
  d3d_device_main_screen = *(IDirect3DDevice9Ex **)get_d3d_device_main_screen();
  v19 = d3d_device_main_screen;
  if ( d3d_device_main_screen )
    ((void (__fastcall *)(IDirect3DDevice9Ex *))d3d_device_main_screen->lpVtbl->AddRef)(d3d_device_main_screen);
  v4 = *(_QWORD *)sub_180456B40();
  v20 = v4;
  if ( v4 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v4 + 8i64))(v4);
  QueryPerformanceCounter(&current_frame_time_counter);
  v5 = get_monitor_check_framerate_maybe();
  target_frame_rate = get_target_frame_rate(v5);
  time_to_sleep = 1000.0 / (double)target_frame_rate
                - (double)(current_frame_time_counter.LowPart - (int)previous_frame_time_counter)
                * 1000.0
                / (double)(int)Frequency.LowPart
                - 3.0;                          // _W_T_F_?!
  if ( time_to_sleep > 0.0 )
    SleepEx((int)time_to_sleep, 0);             // only MS accuracy
  QueryPerformanceCounter(&v15);
  if ( d3d_device_main_screen )
  {
    res_present_ex = ((__int64 (__fastcall *)(IDirect3DDevice9Ex *, _QWORD, _QWORD, _QWORD, _QWORD, _DWORD))d3d_device_main_screen->lpVtbl->TestCooperativeLevel)(// PresentEx
                       d3d_device_main_screen,
                       0i64,
                       0i64,
                       0i64,
                       0i64,
                       0);
    v1 = res_present_ex;
    if ( res_present_ex <= 0 )
    {
      if ( !res_present_ex )
        goto LABEL_16;
      v9 = res_present_ex + 2005530520;
      if ( (unsigned int)(res_present_ex + 2005530520) <= 0xC )
      {
        v10 = 4353;
        if ( _bittest(&v10, v9) )
          goto LABEL_16;
      }
LABEL_15:
      avs2_core_380("Vsync", "PresentEx fail hr=0x%08X", (unsigned int)v1);
      goto LABEL_16;
    }
    if ( res_present_ex != 141953143 && res_present_ex != 141953144 )
      goto LABEL_15;
  }
LABEL_16:
  QueryPerformanceCounter(&v16);
  v14 = 0i64;
  if ( v4 )
  {
    v1 = ((__int64 (__fastcall *)(IDirect3DDevice9Ex *, __int64, __int64 *))d3d_device_sub_screen->lpVtbl->GetRasterStatus)(
           d3d_device_sub_screen,
           1i64,
           &v14);
    if ( v1 >= 0 )
      v1 = (*(__int64 (__fastcall **)(__int64, _QWORD, _QWORD, _QWORD, _QWORD, _DWORD))(*(_QWORD *)v4 + 24i64))(
             v4,
             0i64,
             0i64,
             0i64,
             0i64,
             0);
  }
  QueryPerformanceCounter(&v17);
  v21 = 0ui64;
  v22 = 0ui64;
  v23 = 0ui64;
  v24 = 0ui64;
  if ( d3d_device_main_screen )
    ((void (__fastcall *)(IDirect3DDevice9Ex *, __int128 *))d3d_device_main_screen->lpVtbl->SetCursorPosition)(
      d3d_device_main_screen,
      &v21);
  if ( v4 )
    (*(void (__fastcall **)(__int64, __int128 *))(*(_QWORD *)v4 + 88i64))(v4, &v23);
  v11 = 0;
  if ( qword_18670FBC0 )
    v11 = DWORD1(v21) - DWORD1(xmmword_18670FC30) - (v21 - xmmword_18670FC30);
  if ( qword_18670FBC0 )
    v0 = DWORD1(v23) - DWORD1(xmmword_18670FC50) - (v23 - xmmword_18670FC50);
  *((double *)&xmmword_18670FC70 + 1) = (double)(current_frame_time_counter.LowPart - (int)qword_18670FBE8)
                                      / (double)(int)Frequency.LowPart;
  *(double *)&xmmword_18670FC80 = (double)(v15.LowPart - current_frame_time_counter.LowPart)
                                / (double)(int)Frequency.LowPart;
  *((double *)&xmmword_18670FC80 + 1) = (double)(v16.LowPart - v15.LowPart) / (double)(int)Frequency.LowPart;
  *(double *)&xmmword_18670FC90 = (double)(v17.LowPart - (int)qword_18670FBE8) / (double)(int)Frequency.LowPart;
  *(_QWORD *)&xmmword_18670FC70 = qword_18670FBC0;
  *(__int128 *)((char *)&xmmword_18670FC90 + 8) = v21;
  *(__int128 *)((char *)&xmmword_18670FCA0 + 8) = v22;
  dword_18670FCB8 = v11;
  *(_QWORD *)&xmmword_18670FCC0 = v11 + (_QWORD)xmmword_18670FCC0;
  *((double *)&xmmword_18670FCC0 + 1) = (double)(v17.LowPart - v16.LowPart) / (double)(int)Frequency.LowPart;
  xmmword_18670FCD0 = v23;
  xmmword_18670FCE0 = v24;
  LODWORD(xmmword_18670FCF0) = v0;
  *((_QWORD *)&xmmword_18670FCF0 + 1) += v0;
  qword_18670FD00 = v14;
  PerformanceCount = current_frame_time_counter;
  qword_18670FBD8 = v15.QuadPart;
  previous_frame_time_counter = v16.QuadPart;
  qword_18670FBE8 = v17.QuadPart;
  xmmword_18670FC30 = v21;
  xmmword_18670FC40 = v22;
  xmmword_18670FC50 = v23;
  xmmword_18670FC60 = v24;
  ++qword_18670FBC0;
  if ( v4 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v4 + 16i64))(v4);
  if ( d3d_device_main_screen )
    ((void (__fastcall *)(IDirect3DDevice9Ex *))d3d_device_main_screen->lpVtbl->Release)(d3d_device_main_screen);
  return (unsigned int)v1;
}
```