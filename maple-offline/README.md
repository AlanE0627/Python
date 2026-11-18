# maple-offline

MapleStory GMS v83 오프라인 싱글플레이 Android 단일 APK 프로젝트 (개인 실행용)

- 대상 기기: Galaxy S25+ (arm64-v8a)
- 구성: 클라이언트(C++/GLES3, NativeActivity) + 서버(Cosmic 기반 Java, 앱 내부 서비스) → 127.0.0.1 루프백 연결
- 저장: 앱 내부 SQLite (캐릭터·인벤토리·경험치·퀘스트)

## 폴더 구성

| 폴더 | 내용 | 출처 |
|---|---|---|
| `app/` | 단일 APK 프로젝트: 서버(Java, `app/src/main/java`) + 클라이언트(C++, `app/src/main/cpp`) + 실행 화면(`GameLauncherActivity`) | MapleServerAndroid @ a118987 + OpenMapleClient @ 7dd2f8f 통합 |
| `app/desktop/` | 클라이언트 Linux 테스트 빌드 | — |
| `tools/` | WZ→NX 변환기 Linux판(UTF-16 변환·패딩 이미지 수정), NX 경로 점검 스크립트 | ryantpayton/NoLifeWzToNx @ 73bddde |
| `docs/` | 분석 결과, 작업 로그 | — |

참고 upstream: P0nk/Cosmic @ fec53bc, ryantpayton/MapleStory-Client @ cbb0fe2, brackeen/glfm @ 607fc6f (저장소에 포함, 컨트롤러 입력 패치)

## 에셋 정책

- 게임 에셋(WZ, NX, 설치 파일, exe)은 저장소에 커밋하지 않음 — `.gitignore`로 차단
- 서버용 WZ XML은 upstream MapleServerAndroid에서 빌드 전 복사 (`app/app/src/main/assets/wz/`)
- 클라이언트용 NX는 v83 설치 파일 + Cosmic custom WZ로 로컬 생성 (`tools/wztonx_linux.cpp`)

## 빌드 환경

- JDK 21 (server), JDK 17 (client, Gradle 8.0)
- Android SDK platform 31/35, build-tools 34/35, NDK 26.1.10909125, CMake 3.22.1

## 라이선스

- server/client/tools: AGPL-3.0 (upstream 라이선스 유지)
- BASS 오디오 라이브러리: un4seen 라이선스 (비상업 무료)

## 데스크톱 테스트 빌드 (Linux)

- `app/desktop/`: Android 클라이언트 소스를 그대로 Linux에서 실행하는 테스트용 빌드
- 의존: clang, libc++, GLFW3, Mesa(GLES 3.2/EGL), FreeType, Xvfb
- 실행: `MS_DATA_DIR`(NX 파일·hostip.txt 폴더), `MS_ASSET_DIR`(폰트 폴더) 환경변수 지정
