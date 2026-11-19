#!/bin/bash
# Builds the Korean UI overlay (UI_ko.nx) from KMS 1.2.95 UI.wz/Map.wz.
# Add new sections at the end; keep the existing ones (login, race select,
# v83 creation boards, removed job-guide sign).
set -e
cd /home/claude/kms1295
rm -f out/UI_ko.nx
python3 /home/claude/tools/nx_merge_ui.py --overlay=/home/claude/dl/wz-merged/UI.nx out/UI_ko.nx UI.wz \
  'Login.img!NewChar,NewCharKnight,NewCharAran' MapLogin.img \
  Back/login.img=Map.wz Obj/login.img=Map.wz \
  -MapLogin.img/0/obj/1 'UIWindow.img~' 'StatusBar.img~' 'Basic.img~' "$@"
cp out/UI_ko.nx /home/claude/repos/maple-offline-app/app/src/main/assets/ko/
