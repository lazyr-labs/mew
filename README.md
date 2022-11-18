# Overview

Command line interactive menu with vim-like keybindings.  Supports
regex search (re2) and subsequence search (lz).

This is currently in proof of concept mode.  Still very much a
work in progress.  Most things will change, especially the build
process and project layout.

# Compile
g++ -o mew -O3 -march=native -std=c++20 mew.cpp lz/*.cpp -Ilz -lre2 -lncursesw -ltbb

# Usage
./mew [opts] <files>

or

somecmd | ./mew [opts]

For options, see mew.cpp.  Help hasn't been written yet.

# Examples
find | ./mew
find | ./mew -p # parallel

find ~/ > data.txt
./mew data
cat data | ./mew

# Key bindings

esc: command mode
/: search the menu contents (subsequence default.  Prefix with `/` for regex.)
?: search the input contents (stdin or files given on the command line.
   Subsequence search by default.  Prefix with `/` for regex.)
C: show matching files and line numbers (if input data was not stdin)
H/L: show previous/next menu
x: run interactive command (see below for syntax)
X: run non-interactive command that populates menu (see below for syntax)
space: select item
j/k: scroll down/up
h/l: scroll left/right (text field)
f/F: show search/command history
q: quit and print selected items to stdout

For `x` and `X`, any command that can be written in a terminal is
valid.  There are special printf style placeholders that denote
contents from the menu:

%h: the highlighted item
%s: all selected items
%a: all items

Placeholders for filenames and line numbers will come soon.

For example, running `ls -l %h` in `X` will replace `%h` with the
currently highlighted line and then execute the command.  In `X`
mode, the menu will be populated with the output of the command.
The `x` mode is for interactive commands like less or vim, and the
menu will not be repopulated.  For example, if the lines are file
names, then `vim %s` will open all selected lines in an interactive
vim session.

Key bindings can be changed in the config file.

# Config file

Use `-c <config file>` to remap keys.  Three commands are currently
supported:

remap <x> <y>: maps the functionality of key `y` to `x`
icmd <x> <cmd>: run `cmd` interactively when key `x` is pressed
cmd <x> <cmd>: run `cmd` non-interactively when key `x` is pressed
    and populate the menu with its output

For example, this config makes `j` scroll up, `k` scroll down, `a`
open the highlighted line in `less, and `q` populate the menu with
the output of `ls` run with the selected lines:

```
remap j k
remap k j
icmd a less %h
cmd q ls %s
```
