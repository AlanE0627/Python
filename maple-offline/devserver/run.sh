#!/bin/bash
cd /home/claude/devsrv
exec /usr/lib/jvm/java-21-openjdk-amd64/bin/java -Xmx2g --add-exports java.base/sun.security.action=ALL-UNNAMED --add-opens java.base/java.lang=ALL-UNNAMED -Dorg.slf4j.simpleLogger.defaultLogLevel=info -Dorg.slf4j.simpleLogger.showDateTime=true -cp "classes:$(cat cp.txt)" devmain.DevMain
