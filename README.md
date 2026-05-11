# Suribot

Discord 음성 채널 체류 시간을 기록하고 `/순위`, `/업타임` 명령으로 조회하는 D++ 기반 봇입니다.

## 실행 설정

토큰과 로그 채널 ID는 코드에 넣지 않고 환경변수로 전달합니다.

PowerShell 예시:

```powershell
$env:SURIBOT_TOKEN="YOUR_DISCORD_BOT_TOKEN"
$env:SURIBOT_VOICE_LOG_CHANNEL_ID="123456789012345678"
$env:SURIBOT_DATA_PATH="D:\Suribot\data\user_data.json"
```

`SURIBOT_DATA_PATH`는 선택값입니다. 지정하지 않으면 실행 파일 옆의 `user_data.json`을 사용합니다.

## 명령어

- `/순위`: 누적 업타임 상위 5명을 표시합니다.
- `/업타임 유저:<사용자>`: 특정 사용자의 누적 업타임을 표시합니다.

## 데이터 파일

실제 `user_data.json`은 운영 데이터이므로 Git에 올리지 않습니다. 저장 형식 예시는 `MyBot/user_data.example.json`을 참고하면 됩니다.
