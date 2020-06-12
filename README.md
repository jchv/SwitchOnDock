# SwitchOnDock
SwitchOnDock is a small hack that tries to improve the quality of life of users who dock their laptops by modifying the lid close event.

## How to Use
1. Compile with Visual Studio 2019.

2. Drop binary into Startup folder. (Try hitting <kbd>Windows</kbd>+<kbd>R</kbd> and entering `shell:startup` to get to the Startup folder.)

3. Run it to get started for the first time.

## The Problem
Windows power policies are divided into two partitions: "AC" settings (settings that apply when connected to an AC power source) and "DC" settings (settings that apply on battery power.) This also applies to the lid close event. While other operating systems have more logic around their lid close event to prevent sleeping the device when it is unwanted, Windows simply follows the rules of the currently active power policy, which means closing the lid of a docked laptop with an external mouse, keyboard, and displays will cause it to sleep. This is undesired, but we can do a little better.

## The Solution
SwitchOnDock solves the problem by modifying the current power plan when the device is docked so that nothing happens when the lid is closed, and when it is undocked, it configures the lid close event to sleep.

Currently, it works by modifying the current power plan, which gives less flexibility. A better approach would be to switch between power plans, but I would like to provide a better user experience for this (such as offering to use an existing power plan or create a new one based on your current one, storing the setting in the registry, and offering an actual dialog to reconfigure this.) So for now, it is more hackish than necessary.

SwitchOnDock detects that an external display is connected when there are displays with names other than `\\.\DISPLAY1` connected. It is possible this would not be robust in many circumstances, such as booting into a docked setup.
