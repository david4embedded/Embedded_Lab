import datetime
import asyncio
import sys
import aiocoap.resource as resource
import aiocoap

class TimeResource(resource.Resource):
    async def render_get(self, request):
        # 현재 시간을 문자열로 페이로드에 담아 응답
        print("CoAP message received")
        payload = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S").encode('ascii')
        return aiocoap.Message(payload=payload)

async def main():
    root = resource.Site()
    root.add_resource(['time'], TimeResource())

    # 포트 5683 바인딩
    server_context = await aiocoap.Context.create_server_context(root, bind=('localhost', 5683))
    print("CoAP Server started at coap://localhost:5683/time")
    print("Press Ctrl+C to stop the server.\n")

    try:
        # 서버 무한 대기
        await asyncio.get_running_loop().create_future()
    except asyncio.CancelledError:
        pass
    finally:
        # 종료 처리: 서버 컨텍스트 정리
        await server_context.shutdown()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        # Ctrl + C 입력 시 지저분한 Traceback 없이 깔끔하게 종료
        print("\n[+] CoAP Server successfully stopped.")
        sys.exit(0)