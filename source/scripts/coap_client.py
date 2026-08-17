import asyncio
from aiocoap import *

async def main():
    protocol = await Context.create_client_context()

    # coap://localhost/time 경로로 GET 요청 생성
    request = Message(code=GET, uri='coap://localhost/time')

    try:
        response = await protocol.request(request).response
        print(f"Response Code: {response.code}")
        print(f"Payload: {response.payload.decode('ascii')}")
    except Exception as e:
        print(f"Failed to fetch resource: {e}")

if __name__ == "__main__":
    asyncio.run(main())