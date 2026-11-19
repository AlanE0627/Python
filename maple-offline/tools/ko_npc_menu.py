#!/usr/bin/env python3
"""Replace NPC menu choice labels with short Korean labels (own wording).
Only #L..#l choice labels / one-line menu headings are touched; story
dialogue stays as-is. Applied to both script copies."""
import sys, os
COPIES = ["/home/claude/repos/Cosmic/scripts/npc",
          "/home/claude/repos/maple-offline-app/app/src/main/assets/scripts/npc"]
J = {"Fighter":"파이터","Page":"페이지","Spearman":"스피어맨",
     "Wizard (Fire / Poison)":"위자드(불·독)","Wizard (Ice / Lighting)":"위자드(썬·콜)","Cleric":"클레릭",
     "Hunter":"헌터","Crossbowman":"사수","Assassin":"어쌔신","Bandit":"시프",
     "Brawler":"인파이터","Gunslinger":"건슬링거"}
R = {}
for e,k in J.items():
    R["Please explain to me what being the %s is all about." % e] = "%s 설명 듣기" % k
    for n in range(3):
        R["#L%d#%s\\r\\n" % (n,e)] = "#L%d#%s\\r\\n" % (n,k)
        R["#L%d#%s\")" % (n,e)] = "#L%d#%s\")" % (n,k)
R.update({
 "I'll choose my occupation!": "직업 고르기",
 "I want to make the 3th job advancement.": "3차 전직 하기",
 "Please allow me to do the Zakum Dungeon Quest.": "자쿰 던전 퀘스트 받기",
 "Teach me the skills of my class.": "직업 스킬 배우기",
 "I want to participate in the party quest.": "파티 퀘스트 참가",
 "I would like to \" + (cm.getPlayer().isRecvPartySearchInviteEnabled() ? \"disable\" : \"enable\") + \" Party Search.":
   "파티 찾기 알림 \" + (cm.getPlayer().isRecvPartySearchInviteEnabled() ? \"끄기\" : \"켜기\") + \"",
 "I would like to hear more details.": "설명 듣기",
 "I would like to reclaim a prize.": "보상 다시 받기",
 "Give me Goddess Wristband.": "여신의 팔찌 받기",
 "View current Expedition members": "원정대원 보기",
 "Start the fight!": "전투 시작",
 "Stop the expedition.": "원정대 해산",
 "Lets get this going!": "원정대 만들기",
 "No, I think I'll wait a bit...": "나중에 하기",
 "What would you like to do?#b": "무엇을 할까요?#b",
 "Kerning Square Shopping Center": "커닝 스퀘어",
 "Enter Contruction Site": "공사장 들어가기",
 "#L2#New Leaf City": "#L2#뉴리프 시티",
 "Regular sauna (": "일반 사우나 (",
 "VIP sauna (": "VIP 사우나 (",
 "Choose your destination, for fees will change from place to place.#b": "목적지를 고르세요. 요금은 곳마다 달라요.#b",
 "We have a special 90% discount for beginners.": "초보자는 요금 90% 할인.",
 " mesos)#l": " 메소)#l",
 "Do you want to go to Ellinia?": "엘리니아로 갈까요?",
 "Do you want to go to Ludibrium?": "루디브리엄으로 갈까요?",
 "Do you want to go to Orbis?": "오르비스로 갈까요?",
 "Do you wish to board the flight?": "탑승할까요?",
 "Okay, talk to me if you change your mind!": "마음이 바뀌면 다시 말 걸어 주세요.",
 "I would like to buy 10 for 100,000 Mesos!": "10개 사기(100,000 메소)",
 "No thanks, let me in now!": "바로 들어가기",
})
files = sys.argv[1:]
tot = 0
for d in COPIES:
    for f in files:
        p = os.path.join(d, f + ".js")
        if not os.path.exists(p): print("missing", p); continue
        s = open(p, encoding="utf-8").read(); o = s; c = 0
        for a,b in R.items():
            if a in s: c += s.count(a); s = s.replace(a,b)
        if s != o: open(p,"w",encoding="utf-8").write(s)
        print(d.split('/')[3], f, c); tot += c
print("total", tot)
