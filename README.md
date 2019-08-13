# OpenVR Haptic Injector

A small utility that intercepts OpenVR haptic events and transforms them into low frequency sinusoidal signals sent to a secondary sound card

Suitable for controlling haptic devices based on bass shakers / body shakers, such as haptic suits or simulator rigs

# How to use

Open SteamVR first!

Execute "OpenVR Haptic Injector.bat", likely the first time it will not find a configured sound card, don't panic, take note of the sound card list on the screen, choose the one connected to the haptic system, open config.ini and insert in audiodevice entry a unique string taken from ID or Description. At the second start it should perform a short sound test on that card

# Mapping events

Map is a sound sent to the sound card when a haptic event occurs, can be chosen from a library and modified in duration (loop) and volume

Play your game, all detected haptic events will be shown, with Amplitude, Duration and Frequency values. Initially the program doesn't know any events and will send a default map (see the [default] section of config.ini) for any events.

Take note to the last string printed below the events, ie:

```
0.3750000.000000200.000000
```

This is an event, so in config.ini, in the [maps] section we can insert a line like this:

```
[maps]
0.3750000.000000200.000000=1,3,0.8
```

This means: send the audio with index 1, three times in loop, with volume set to 0.8

# Audio Library

The default library is waves.xwb, a Wave Banks. You can edit this (using waves.xap) or create more using XACT, the [Cross-platform Audio Creation Tool](https://en.wikipedia.org/wiki/Cross-platform_Audio_Creation_Tool). These libraries are loaded in memory and are very fast, for the least possible latency. The default library currently contains 6 very short waveforms, indexed from 0 to 5

```
0 - 40Hz
1 - 50Hz
2 - 60Hz
3 - 80Hz
4 - 100Hz
5 - 120Hz
```

![Cross-platform Audio Creation Tool](https://github.com/mmorselli/OpenVR-Haptic-Injector/blob/master/XACT.png)

you can easily create new waveforms with free programs like Audacity


# Using profiles

Different games and devices may require a different reaction to haptic events, you can then create different profiles. Copy config.ini to a new file (i.e. my-game-on-static-rig.ini ), change the profile name in the [main] section (i.e. profile=My Game On Static Rig) and configure the new mapping. If you wish you can also define a different .xwb file in the [default] section. Start the program with the new .ini as the first argument

```
"OpenVR Haptic Injector.exe" my-game-on-static-rig.ini
```

