import argparse
import getpass  # 1. getpass 모듈 임포트
from email.mime.text import MIMEText
import smtplib


def main():
  parser = argparse.ArgumentParser(
      description='파이썬 네이버 SMTP 이메일 전송 스크립트'
  )
  parser.add_argument(
      '-s', '--sender', required=True, help='보내는 사람 네이버 이메일'
  )
  parser.add_argument(
      '-r', '--receiver', required=True, help='받는 사람 이메일 주소'
  )
  # 💡 -p 인자(password)를 아예 파서에서 빼버립니다!

  parser.add_argument(
      '--subject', default='네이버 SMTP 테스트', help='이메일 제목'
  )
  parser.add_argument(
      '--body', default='네이버로 발송된 테스트 메일입니다.', help='이메일 본문'
  )

  args = parser.parse_args()

  # 2. 실행 중에 터미널에서 비밀번호를 안전하게 입력받음 (화면에 안 보임)
  password = getpass.getpass('네이버 비밀번호 입력: ')

  msg = MIMEText(args.body, 'plain', 'utf-8')
  msg['Subject'] = args.subject
  msg['From'] = args.sender
  msg['To'] = args.receiver

  try:
    print('네이버 SMTP 서버(smtp.naver.com:465)에 연결 중...')

    with smtplib.SMTP_SSL('smtp.naver.com', 465) as server:
      server.login(args.sender, password)  # 안전하게 입력받은 변수 사용
      server.sendmail(args.sender, args.receiver, msg.as_string())

    print(f"성공: '{args.receiver}'(으)로 네이버 메일이 발송되었습니다!")

  except Exception as e:
    print(f'발송 실패: {e}')


if __name__ == '__main__':
  main()