# Micro stuttering and bad frame pacing

## 

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