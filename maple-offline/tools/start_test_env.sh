#!/bin/bash
# Restart the desktop test environment (virtual display, DB, test server).
pgrep Xvfb >/dev/null || (Xvfb :99 -screen 0 1560x720x24 >/tmp/xvfb.log 2>&1 &)
mkdir -p /run/mysqld && chown mysql:mysql /run/mysqld 2>/dev/null
pgrep mariadbd >/dev/null || (mysqld_safe --user=mysql >/tmp/mysql.log 2>&1 &)
sleep 5
if ! pgrep -f Cosmic.jar >/dev/null; then
  (cd /home/claude/repos/Cosmic && nohup /usr/lib/jvm/java-21-openjdk-amd64/bin/java -Xmx3g -jar target/Cosmic.jar >/tmp/cosmic.log 2>&1 &)
  for i in $(seq 1 40); do sleep 3; grep -q "is now online" /tmp/cosmic.log && break; done
fi
grep -o "Cosmic is now online.*" /tmp/cosmic.log | tail -1
