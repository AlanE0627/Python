# PC에서 폰 서버 그대로 돌리기

- 앱 서버 Java 소스(app/src/main/java, com/·openMapleClient/ 제외)를 그대로 컴파일
- shim/: android.database.sqlite(SQLite JDBC 위), ContentValues, Context/AssetManager(앱 assets 폴더), Point/Rect/RectF, Log, LogBackConfig 대체
- 필요 jar: sqlite-jdbc, slf4j-simple, 앱 build.gradle 의존성(netty 4.1.100 개별 모듈, jackson, rhino-android + rhino 1.7.10, jdbi 등)
- 실행: run.sh (DevMain → Server.main), DB는 db/cosmic(첫 실행 시 assets/sql/cosmic 복사)
