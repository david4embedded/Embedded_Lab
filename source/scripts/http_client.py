import socket

# 1. TCP 소켓 생성
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# 2. 테스트용 공공 API 서버에 연결 (httpbin.org는 HTTP 테스트용으로 아주 좋습니다)
host = "httpbin.org"
client_socket.connect((host, 80))

# 3. HTTP 규격에 맞는 텍스트 요청 메시지 직접 작성 (Raw HTTP Request)
# 주의: 끝에 빈 줄(\r\n\r\n)이 들어가야 헤더가 끝나고 서버가 본문 처리를 시작합니다.
request_line = f"GET /get?message=hello_http HTTP/1.1\r\n"
headers = f"Host: {host}\r\n" "Connection: close\r\n\r\n"
http_request = request_line + headers

# 4. 서버로 전송
client_socket.sendall(http_request.encode("utf-8"))

# 5. 서버가 보내온 응답을 받아서 출력
response = b""
while True:
  data = client_socket.recv(4096)
  if not data:
    break
  response += data

# 6. 소켓 닫기
client_socket.close()

# 결과 출력 (HTTP 응답 전체가 그대로 찍힙니다)
print(response.decode("utf-8"))