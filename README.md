# dexpo

Desktop Expose -- a tool that lets you view all of your X11 virtual desktops
and navigate between them

## Description:

**dexpo** (Desktop Exposer) is an application similar to MacOS's _expose_,
windows's `Win+Tab` desktop switcher or KDE Plasma's _Pager_. It displays
screenshots of your virtual desktops (aka workspaces) and simplifies navigation
between them.

> Why do I need it?

If you have a tiling non-compositing WM and use a lot of workspaces, it gets
pretty easy to get lost. With **dexpo** you can find the workspace you are
looking for at a glance and switch to it.

## Features:

-   Fast. Written with `xcb` and won't slow down your window manager.
-   Customizable. Config file is a `.hpp` header file. Program is recompiled after every reconfiguration.
-   Can select displays with a mouse click or keyboard

![dexpo_preview_horizontal](https://user-images.githubusercontent.com/47359245/120087036-40b02580-c0ed-11eb-8181-d43d82420a2f.png)

Vertical stacking
example:[dexpo_preview_vertical](https://user-images.githubusercontent.com/47359245/120087034-3db53500-c0ed-11eb-8071-0b7d536d7ea3.png)

## Limitations:

It's impossible to screenshot inactive desktop with X11. This is why a daemon
has to be used. It screenshots your desktops when you open them and downsizes
them.
