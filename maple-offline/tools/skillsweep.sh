#!/bin/bash
# usage: skillsweep.sh "<job> <skill> <skill>..." ...   (client must be in game)
export DISPLAY=:99
chat() { xdotool key Return; sleep 0.6; xdotool type --delay 50 "$1"; xdotool key Return; sleep 1.5; }
for grp in "$@"; do
  set -- $grp; job=$1; shift
  chat "!job $job"; chat "!heal"; chat "!killall"; chat "!spawn 1210100 4"; sleep 1
  for sk in "$@"; do
    echo $sk > /tmp/ms_skill
    n0=$(wc -l < /tmp/client.log)
    xdotool key F11; sleep 2.2
    import -window root /tmp/sk_$sk.png
    echo "=== job $job skill $sk"
    awk -v n=$n0 'NR>n && (/use_move|Sent Packet|Received Packet|Error|cannot/) && !/MOVE_PLAYER|MOVE_MONSTER|MOVE_LIFE|MOVE_PET/' /tmp/client.log | sort | uniq -c | head -12
    sleep 1.5
  done
done
rm -f /tmp/ms_skill
