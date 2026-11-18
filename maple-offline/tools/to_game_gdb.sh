#!/bin/bash
# Run client under gdb, log in, enter the game with the default character.
pkill -f ms_desktop; sleep 1
cd /home/claude/msdata && (env "$@" DISPLAY=:99 MS_WIDTH=1560 MS_HEIGHT=720 MS_DATA_DIR=/home/claude/msdata MS_ASSET_DIR=/home/claude/repos/maple-offline-app/app/src/main/assets timeout 900 gdb -batch -ex run -ex "bt 12" --args /home/claude/bd/ms_desktop > /tmp/gdb.log 2>&1 &)
sleep 55; export DISPLAY=:99
xdotool mousemove 950 282 click 1; sleep 1; xdotool type --delay 80 tester
xdotool mousemove 950 322 click 1; sleep 1; xdotool type --delay 80 tester; xdotool key Return; sleep 14
xdotool mousemove 555 200 click 1; sleep 3; xdotool mousemove 1002 279; sleep 0.5; xdotool click 1; sleep 8
xdotool mousemove 1550 600; xdotool key Return; sleep 25
