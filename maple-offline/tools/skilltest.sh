#!/bin/bash
# usage: skilltest.sh <jobid> <spclicks_row3> <spclicks_row4> <scrolls> <outpng>
# assumes the client is in game; assigns visible row 3 skill to G and row 4 to F
export DISPLAY=:99
JOB=$1; R3=$2; R4=$3; SC=$4; OUT=$5
xdotool key Return; sleep 1; xdotool type --delay 60 "!job $JOB"; xdotool key Return; sleep 2
xdotool key Return; sleep 1; xdotool type --delay 60 '!sp 30'; xdotool key Return; sleep 1.5
xdotool mousemove 1332 698 click 1; sleep 1.2
xdotool key k; sleep 1.2
for i in $(seq 1 $SC); do xdotool mousemove 305 428 click 1; sleep 0.5; done
for i in $(seq 1 $R3); do xdotool mousemove 282 353 click 1; sleep 0.5; done
for i in $(seq 1 $R4); do xdotool mousemove 282 401 click 1; sleep 0.5; done
drag() { xdotool mousemove $1 $2; sleep 0.8; xdotool mousedown 1; sleep 0.5; xdotool mousemove $(( ($1+$3)/2 )) $(( ($2+$4)/2 )); sleep 0.8; xdotool mousemove $3 $4; sleep 0.8; xdotool mouseup 1; sleep 1; }
drag 145 345 357 242   # row 3 -> G
drag 145 393 330 280   # row 4 -> V
xdotool mousemove 0 0; sleep 0.3; import -window root ${OUT%.png}_keys.png
xdotool mousemove 735 358 click 1; sleep 1
xdotool key k; sleep 0.8
xdotool key Return; sleep 1; xdotool type --delay 60 '!spawn 100101 3'; xdotool key Return; sleep 2
xdotool key Left; sleep 0.3; xdotool key g; sleep 1.5; xdotool mousemove 0 0; import -window root ${OUT%.png}_g.png
sleep 1; xdotool key v; sleep 1.5; import -window root ${OUT%.png}_v.png
