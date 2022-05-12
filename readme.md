# Keyboard layout switching using alt-shift
## What is altshift
Altshift is a Linux tool that switches the keyboard layout using alt-shift and does not break column mode editing (aka block-selection) of certain text editors.  
For example it is compatible with the column mode editing in Geany.
## What problem does it solve
Using alt-shift for keyboard layout switching works fine in the Visual Studio and Notepad++ editors.  
Unfortunately this is not true on Linux.  
Typically keyboard switching is done using Xorg's built in functionality:
```
setxkbmap "us,bg" ",phonetic" "grp:alt_shift_toggle"
```
Unfortunately this breaks block mode selection in text editors like Geany.  
One possible solution is to use another key combination for keyboard layout switching:
```
setxkbmap "us,bg" ",phonetic" "grp:alt_caps_toggle"
```
The altshift tool provides another solution.
## Installation
Use the Makefile or your favorite compiler:
```
cc -o altshift altshift.c
```
Start manually from within X session, providing your keyboard input device:
```
sudo ./altsift if=/dev/input/event0
```
Ensure that _alshift_ starts with the X session in other means.  
  
Show command line options:
```
./altshift -h
```
## Caveats
- you should provide the keyboard input device file
- _altshift_ should be able to read the input device so it may need root privileges

