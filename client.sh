CLIENT_FILE="client.c"
CLIENT_EXEC="client"
IP="127.0.0.1"
PORT=9190

echo "🔨 클라이언트 컴파일 중..."
gcc -o $CLIENT_EXEC $CLIENT_FILE -lncursesw

if [[ $? -ne 0 ]]; then
    echo "❌ 클라이언트 컴파일 실패"
    exit 1
fi

echo "✅ 클라이언트 실행 중"
./$CLIENT_EXEC $IP $PORT