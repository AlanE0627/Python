#!/bin/bash
# usage: with_weapon.sh <weapon itemid>  -> relaunch client in game with that weapon equipped (DB swap while offline)
pkill -f ms_desktop; sleep 6
mysql -u root cosmic -e "update inventoryitems set itemid=$1 where characterid=4 and position=-11 and inventorytype=-1;"
bash /home/claude/scripts/to_charselect.sh MS_HIDE_TOUCH=1 MS_DEBUG_SKILL=1004 >/dev/null 2>&1
export DISPLAY=:99; xdotool key Return; sleep 10
