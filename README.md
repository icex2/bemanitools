# WIP SCRATCHPAD

* further optimzation potential to remove the last micro stutters:
  * at least on old styles, background videos are read/streamed from file and decoded in main
    thread -> have in memory file for videos to load them entirely into ram before song
    -> most apparent stutter is the one right when the video starts
  * tested 11 without videos by just removing the movie folder, game works fine without. still microstutter occasionally

* set main (render) thread priority to max
* remove CPU yield on frame pacer and also the sleep hook -> check if these were causing further frame stutter

* realtime priority process and threads via inject and .bat scripts (for launcher games)
  has significant impact on ezusb timings and data races

* realtime priority process fixes a significant amount of microstuttering
  * seems to fix also ezusb timeout issues?

* thread/process priority for iidx games
  * createprocesds in inject needs to set the priority class otherwise setting realtime priority to inject doesn't make the target game go into realtime prioritity

* check/test if processor affinity for main thread helps

* yoink mixer.c from popnhook and port to iidx to block volume settings changes

iidx genuine code base bug: frame pacing depends on fixed framerate as close to 60hz as possible -> gameplay stuttering caused by the game re-syncing the notes. more notable the further the monitor refresh rate is away from 60hz

* FM trns out happening on empress
  * also on DJT
  * gold works -> not anymore now...

* ezusb fm trns-out error needs to be fixed, happens too often -> FM = firmware
* log output flood cross-impact: ezusb timeout seems to be impacted by too much log output for some reason
  * on both, launcher and inject -> hiding cmd window helps
  * document that as a known issue in btools and how to turn log level down to "fix" this
  * this also might have an impact on the SQ errors on 10th style
  * just tested with minimized window and nope, also happened
  * trns out timeouts seem to happen more often on ezusb2 than on 1

  * handbook faq: how does syncing work across the games? -> 09-19 use btools "monitor-check" and chart patching feature,
    all newer HD style games starting 20+ use the built in monitor check feature as it can determine arbitrary refresh
    rates and applies them accordingly. stock old games only apply either 59.95 or 60.05 which often aren't even close
    to modern monitor refresh rates

    * btools dev journal ezusb? 9th style led ticker scrolling is very bouncy -> non fixable problem because of the
    out buffer not being synchronized correctly. symptom of the bad concurrency job in ezusb client library

## TODO merge with journal about frame pacing issue

konami apparently never did implement proper frame pacing logic to ensure clean sync of any monitor hardware/GPU hardware connected to the system with the engine sync (target refresh rate in charts)

* 60: 16,6666666
* 59.98 -> 16,67222
* 59.9345 -> 16,68488

the old games, pre tricoro era, do not have any frame pacing and rely entirely on hardware, GPU+monitor refresh rate to pace accordingly. if this is not given, then stuff goes offsync and stuttering appears due to too short frames. this became more and more of an issue the more different the hardware got and the more different types of monitors were introduced since there was not a single timing matching anymore properly. instead of proper framepacing, the devs apparently tried to address this by approximating things starting iidx gold with their "monitor check" feature. they either had the game tick/run on 59.95 or 60.05 hz. the closer any monitor was to these values, the better the sync and less stuttering/frame pacing issues. this got addressed more properly but still not proper enough with tricoro+ where they implemented proper timing logic that times frames accordingly to sync up with the monitor refresh rate. however, the accuracy is limiated to 1 ms. the songs are sync this way (or the offsync is so minor, you don't notice), but the frame pacing is still an issue especially the farther the actual refresh rate is away from any refresh rate dividable by full ms accuracy

## TODO add to handbook

[2023/04/09 22:30:14] W:libutils: '\\Mac\VM Shared Folder\iidx\25\bm2dx.dll' couldn't be loaded: The specified module could not be found.

[2023/04/09 22:30:14] F:libutils: 

Please check if bm2dx.dll exists and the permissions are fine.
If the problem still persists, try installing:
* Microsoft Visual C++ Redistributables (*all* versions, *both* x86/x64)
    Hint: use one of the AIO installers (e.g. https://github.com/abbodi1406/vcredist)
* DirectX End-User Runtimes (June 2010)
* Running Windows 10 "N"?
    Grab: https://www.microsoft.com/en-us/software-download/mediafeaturepack
    Check: https://support.microsoft.com/en-us/help/4562569/media-feature-pack-for-windows-10-n-may-2020
* Running Windows 7 "N"?
    Grab: https://www.microsoft.com/en-us/download/details.aspx?id=16546
* Still have problems?
Find the missing dependency using: http://www.dependencywalker.com/



# Bemanitools 5
[![pipeline status](https://dev.s-ul.net/djhackers/bemanitools/badges/master/pipeline.svg)](https://dev.s-ul.net/djhackers/bemanitools/commits/master)

Version: 5.45

[Release history](CHANGELOG.md)

A collection of tools to run [various Bemani arcade games](#supported-games).

Bemanitools 5 (BT5) is the successor to Bemanitools 4 which introduces a big code cleanup and support for newer games.
BT5 uses a cleaner approach than BT4 did; specifically, all input and lighting is handled by emulating the protocols
spoken by the real IO PCBs, instead of replacing chunks of game code like BT4. The benefits of this approach are a more
authentic gameplay experience, and easier support for a broader range of releases from each game series.

## Documentation
Browse our [documentation](doc/README.md) as it might already cover various questions and concerns
you are looking for or about to ask.

## Contributions and bug reporting
[Read the dedicated CONTRIBUTING.md documentation](CONTRIBUTING.md).

The tl;dr version and golden rules of the sections in the document:
* **EVERYONE** can contribute, this is **NOT** limited to people coding
* [Open an issue on gitlab for discussions, feature requests and bug reports](CONTRIBUTING.md#reporting-and-discussions-issues-section-on-github)
* [ALWAYS report bugs as issues and ALWAYS use the available bug template](CONTRIBUTING.md#bug-reports)
* [Everyone is allowed to submit changes which are not just limited to code by opening merge requests](CONTRIBUTING.md#pull-requests-bugfixes-new-features-or-other-code-contributions)
* [Documentation improvements can and even should be contributed by non developers](CONTRIBUTING.md#pull-requests-bugfixes-new-features-or-other-code-contributions)

## Supported games
The following games are supported with their corresponding hook-libraries.

* BeatStream
    * BeatStream (`bst.zip`): bsthook
    * BeatStream アニムトライヴ (`bst.zip`): bsthook
* [Dance Dance Revolution](doc/ddrhook/README.md)
    * Dance Dance Revolution X (`ddr-11.zip`): [ddrhook1](doc/ddrhook/ddrhook1.md)
    * Dance Dance Revolution X2 (US/EU regions) (`ddr-12-us.zip`): [ddrhook1](doc/ddrhook/ddrhook1.md)
    * Dance Dance Revolution X2 (JP region) (`ddr-12.zip`): [ddrhook2](doc/ddrhook/ddrhook2.md)
    * Dance Dance Revolution 2013 (`ddr-14-to-16.zip`): [ddrhook2](doc/ddrhook/ddrhook2.md)
    * Dance Dance Revolution 2014 (`ddr-14-to-16.zip`): [ddrhook2](doc/ddrhook/ddrhook2.md)
    * Dance Dance Revolution A (`ddr-14-to-16.zip`): [ddrhook2](doc/ddrhook/ddrhook2.md)
* [Beatmania IIDX](doc/iidxhook/README.md)
    * Beatmania IIDX 9th Style (`iidx-09-to-12.zip`): [iidxhook1](doc/iidxhook/iidxhook1.md)
    * Beatmania IIDX 10th Style (`iidx-09-to-12.zip`): [iidxhook1](doc/iidxhook/iidxhook1.md)
    * Beatmania IIDX 11 IIDX RED (`iidx-09-to-12.zip`): [iidxhook1](doc/iidxhook/iidxhook1.md)
    * Beatmania IIDX 12 HAPPY SKY (`iidx-09-to-12.zip`): [iidxhook1](doc/iidxhook/iidxhook1.md)
    * Beatmania IIDX 13 DistorteD (`iidx-13.zip`): [iidxhook2](doc/iidxhook/iidxhook2.md)
    * Beatmania IIDX 14 GOLD (`iidx-14-to-17.zip`): [iidxhook3](doc/iidxhook/iidxhook3.md)
    * Beatmania IIDX 15 DJ TROOPERS (`iidx-14-to-17.zip`): [iidxhook3](doc/iidxhook/iidxhook3.md)
    * Beatmania IIDX 16 EMPRESS (`iidx-14-to-17.zip`): [iidxhook3](doc/iidxhook/iidxhook3.md)
    * Beatmania IIDX 17 SIRIUS (`iidx-14-to-17.zip`): [iidxhook3](doc/iidxhook/iidxhook3.md)
    * Beatmania IIDX 18 Resort Anthem (`iidx-18.zip`): [iidxhook4](doc/iidxhook/iidxhook4.md)
    * Beatmania IIDX 19 Lincle (`iidx-19.zip`): [iidxhook5](doc/iidxhook/iidxhook5.md)
    * Beatmania IIDX tricoro CN (狂热节拍 IIDX 2) (`iidx-20-cn.zip`): [iidxhook5-cn](doc/iidxhook/iidxhook5-cn.md)
    * Beatmania IIDX 20 Tricoro (`iidx-20.zip`): [iidxhook6](doc/iidxhook/iidxhook6.md)
    * Beatmania IIDX 21 SPADA (`iidx-21-to-24.zip`): [iidxhook7](doc/iidxhook/iidxhook7.md)
    * Beatmania IIDX 22 PENDUAL (`iidx-21-to-24.zip`): [iidxhook7](doc/iidxhook/iidxhook7.md)
    * Beatmania IIDX 23 copula (`iidx-21-to-24.zip`): [iidxhook7](doc/iidxhook/iidxhook7.md)
    * Beatmania IIDX 24 SINOBUZ (`iidx-21-to-24.zip`): [iidxhook7](doc/iidxhook/iidxhook7.md)
    * Beatmania IIDX 25 CANNON BALLERS (`iidx-25-to-26.zip`): [iidxhook8](doc/iidxhook/iidxhook8.md)
    * Beatmania IIDX 26 Rootage (`iidx-25-to-26.zip`): [iidxhook8](doc/iidxhook/iidxhook8.md)
    * Beatmania IIDX 27 Heroic Verse (`iidx-27-to-29.zip`): [iidxhook9](doc/iidxhook/iidxhook9.md)
    * Beatmania IIDX 28 BISTROVER (`iidx-27-to-29.zip`): [iidxhook9](doc/iidxhook/iidxhook9.md)
    * Beatmania IIDX 29 CASTHOUR (`iidx-27-to-29.zip`): [iidxhook9](doc/iidxhook/iidxhook9.md)
* [jubeat](doc/jbhook/README.md)
    * jubeat (`jb-01.zip`): [jbhook1](doc/jbhook/jbhook1.md)
    * jubeat ripples (`jb-02.zip`): [jbhook1](doc/jbhook/jbhook1.md)
    * jubeat knit (`jb-03.zip`): [jbhook2](doc/jbhook/jbhook2.md)
    * jubeat copious (`jb-04.zip`): [jbhook2](doc/jbhook/jbhook2.md)
    * jubeat saucer (fulfill) (`jb-05-to-07.zip`): [jbhook3](doc/jbhook/jbhook3.md)
    * jubeat prop (`jb-05-to-07.zip`): [jbhook3](doc/jbhook/jbhook3.md)
    * jubeat qubell (`jb-05-to-07.zip`): [jbhook3](doc/jbhook/jbhook3.md)
    * jubeat clan (`jb-08.zip`): [jbhook3](doc/jbhook/jbhook3.md)
    * jubeat festo (`jb-08.zip`): [jbhook3](doc/jbhook/jbhook3.md)
* [pop'n music](doc/popnhook/README.md)
    * pop'n music 15 ADVENTURE (`popn-15-to-18.zip`) using [popnhook1](doc/popnhook/popnhook1.md)
    * pop'n music 16 PARTY♪ (`popn-15-to-18.zip`) using [popnhook1](doc/popnhook/popnhook1.md)
    * pop'n music 17 THE MOVIE (`popn-15-to-18.zip`) using [popnhook1](doc/popnhook/popnhook1.md)
    * pop'n music 18 せんごく列伝 (`popn-15-to-18.zip`) using [popnhook1](doc/popnhook/popnhook1.md)
* SOUND VOLTEX
    * SOUND VOLTEX BOOTH (`sdvx-01-to-04.zip`): sdvxhook
    * SOUND VOLTEX II -infinite infection- (`sdvx-01-to-04.zip`): sdvxhook
    * SOUND VOLTEX III GRAVITY WARS (`sdvx-01-to-04.zip`): sdvxhook
    * SOUND VOLTEX IV HEAVENLY HAVEN (`sdvx-01-to-04.zip`): sdvxhook
    * SOUND VOLTEX Vivid Wave (`sdvx-05-to-06`): sdvxhook2
    * SOUND VOLTEX EXCEED GEAR (`sdvx-05-to-06`): sdvxhook2

## Auxiliary tooling
* Bootstrapping
  * [inject](doc/inject.md): Inject arbitrary hooking libraries into a target application process.
  * [launcher](doc/launcher.md): Bootstrap Konami's AVS environment and launch a target application with arbitrary
    injected hooking libraries. 
* Beatmnia IIDX Ezusb IO board
  * [ezusb-iidx-fpga-flash](doc/tools/ezusb-iidx-fpga-flash.md): Flash a binary blob with FPGA firmware to a target
    ezusb FX IO board
  * [ezusb-iidx-sram-flash](doc/tools/ezusb-iidx-sram-flash.md): Flash a binary blob with SRAM contents to a target
    ezusb FX2 IO board
* Exit hooks: Exit the game with a button combination using native cabinet inputs
  * [iidx-ezusb-exit-hook](doc/tools/iidx-ezusb-exit-hook.md): For IIDX with ezusb IO
  * [iidx-bio2-exit-hook](doc/tools/iidx-bio2-exit-hook.md): For IIDX with BIO2 IO
  * [iidx-ezusb2-exit-hook](doc/tools/iidx-ezusb-exit-hook.md): For IIDX with ezusb FX2 IO
* Bemanitools API testing: Tools for testing bemanitools API implementations
  * [eamiotest](doc/tools/eamiotest.md): For [eamio API](doc/api.md#eamuse-readers)
  * [iidxiotest](doc/tools/iidxiotest.md): For [iidxio API](doc/api.md#io-boards)
  * [jbiotest](doc/tools/jbiotest.md): For [jbio API](doc/api.md#io-boards)
* [aciotest](doc/tools/aciotest.md): Command line tool to quickly test ACIO devices
* config: UI input/output configuration tool when using the default bemanitools API (geninput)
* ir-beat-patch-9/10: Patch the IR beat phase on IIDX 9 and 10
* [mempatch-hook](doc/tools/mempatch-hook.md): Patch raw memory locations in the target process based on the provided
  configuration
* [pcbidgen](doc/tools/pcbidgen.md): Konami PCBID generator tool
* [ViGEm clients](doc/vigem/README.md): Expose BT5 APIs as XBOX game controllers to play any games with real cabinet
  hardware.

## Pre-requisites

### Supported platforms
Our main platforms are currently Windows XP and Windows 7 which are also the target platforms on the original hardware
of those games. However, as it gets more difficult to get and maintain hardware comptible with Windows XP, this might
change in the future. Many games also run on very recent Windows 10 builds but bear with us that it's hard to keep up
with Windows updates breaking legacy software.

### Distribution contents
Check the [list of supported games](#supported-games) to grab the right files for your game. BT5 also includes
a *tools* subpackage (tools.zip) as well as the full source code (src.zip).

You will find *.md files in various sub-packages that give you further instructions for setup, usage, error information
or FAQ. We advice you to read them as your questions and concerns might already be answered by them. If not, let us
know if there is any information that you consider helpful or important to know and should be added.

### Setup and dependencies
Most (older generation) games were developed for Windows XP Embedded but should run fine on any
consumer version of Windows XP. Newer versions of Windows, e.g. Windows 7, 8 and 10, should be fine
as well. Some hooks also include fixes required to run the games on a more recent version.

Depending on the game, you also need the following dependencies installed:

* The 32-bit (x86) version of
  [Microsoft Visual C++ 2010 Service Pack 1 Redistributable Package MFC Security Update](https://www.microsoft.com/en-sg/download/details.aspx?id=26999)
* The 32-bit (x86) and 64-bit (x64) versions of
  [Microsoft Visual C++ Redistributable Packages for Visual Studio 2013](https://www.microsoft.com/en-sg/download/details.aspx?id=40784)
* The [DirectX 9 End-User Runtimes (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=8109)

See also
[bemanitools-supplement](https://www.github.com/djhackersdev/bemanitools-supplement/)
for files.

## Development
### Building
See the [development document](doc/development.md).

### Architecture
A dedicate [architecture document](doc/architecture.md) outlines the architecture of Bemanitools and points out the most
important aspects you should know before you get started with development.

### API
Please refer to the [API documentation](doc/api.md).

## Release process
Please refer to the [dedicated documentation](doc/release-process.md).

## License
Source code license is the Unlicense; you are permitted to do with this as thou wilt. For details, please refer to the
[LICENSE file](LICENSE) included with the source code.






