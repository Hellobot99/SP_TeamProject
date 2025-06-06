# 멀티플레이어 좀비 아포칼립스 텍스트 어드벤처 게임

## 한 줄 소개
C와 ncurses로 구현된 협동형 좀비 아포칼립스 생존 텍스트 어드벤처 게임

## 📚 목차

1. [제목](#제목)  
2. [한 줄 소개](#한-줄-소개)  
3. [목차](#목차)  
4. [소개 (Introduction)](#소개-introduction)  
5. [주요 기능 (Features)](#주요-기능-features)  
6. [설치 및 빌드 (Installation-&-Build)](#설치-및-빌드)  
7. [사용 예시 (Usage-/Demo)](#사용-예시-usage--demo)  
8. [프로젝트 구조 (Project-Structure)](#프로젝트-구조-project-structure)  
9. [아키텍처 및 코드 설명 (Architecture-&-Code-Overview)](#아키텍처-및-코드-설명-architecture--code-overview)  
10. [저자](#저자)  
11. [참조](#참조)  

## 제목
(“제목” 섹션 내용을 여기에 작성)

## 한 줄 소개
(“한 줄 소개” 섹션 내용을 여기에 작성)

## 목차
(실제로는 이미 위에 목차를 넣었으므로 이 헤더는 생략하거나,  
본문에서는 다른 용도로 쓸 수 있습니다.)

## 소개 (Introduction)
(프로젝트 배경·목적·기술 스택 등 서술)

## 주요 기능 (Features)
- **멀티플레이어 협동**: …
- **실시간 채팅**: …
- **다수결 투표 시스템**: …
- **UTF-8 한글 출력 지원**: …
- **유저 상태 관리**: …

## 설치 및 빌드
```bash
# 필요한 라이브러리 설치 (Ubuntu 예시)
sudo apt update
sudo apt install build-essential libncursesw5-dev

# 리포지토리 클론
git clone https://github.com/USERNAME/REPO.git
cd REPO

# 서버 빌드
cd server
make

# 클라이언트 빌드
cd ../client
make
