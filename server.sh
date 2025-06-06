SERVER_FILE="server.c"
SERVER_EXEC="server"
PORT=9190

echo "서버 컴파일 중..."
gcc -o $SERVER_EXEC $SERVER_FILE

if [[ $? -ne 0 ]]; then
    echo "서버 컴파일 실패"
    exit 1
fi

echo "서버 실행 중 (포트: $PORT)"
./$SERVER_EXEC $PORT