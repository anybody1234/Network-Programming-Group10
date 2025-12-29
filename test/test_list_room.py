import socket
import json
import time

# Cấu hình kết nối server
HOST = '127.0.0.1'
PORT = 5500

def send_json(sock, data):
    """Hàm gửi dữ liệu JSON kèm ký tự kết thúc \r\n"""
    try:
        # 1. Chuyển đổi dict sang chuỗi JSON
        json_str = json.dumps(data)
        
        # 2. Thêm ký tự kết thúc \r\n (QUAN TRỌNG VỚI SERVER CỦA BẠN)
        message = json_str + "\r\n"
        
        # 3. Gửi đi
        print(f"[Client] Đang gửi: {json_str}")
        sock.sendall(message.encode('utf-8'))
        
        # 4. Nhận phản hồi
        response = sock.recv(4096)
        response_str = response.decode('utf-8').strip()
        print(f"[Server] Phản hồi: {response_str}\n")
        return json.loads(response_str) if response_str else None
    except Exception as e:
        print(f"Lỗi khi gửi/nhận: {e}")
        return None

def main():
    try:
        # Bước 1: Kết nối đến Server
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))
        print(f"Đã kết nối đến {HOST}:{PORT}\n")
        
        # Nhận tin nhắn chào mừng (Welcome message)
        welcome = s.recv(1024)
        print(f"[Server] Welcome: {welcome.decode('utf-8').strip()}\n")
        # ---------------------------------------------------------
        # BƯỚC 3: ĐĂNG NHẬP (LOGIN)
        # Server yêu cầu phải đăng nhập mới được tạo phòng
        # ---------------------------------------------------------
        login_payload = {
            "type": "LOGIN",
            "username": "admin",   # Thay bằng user có trong DB của bạn
            "password": "123456"
        }
        
        resp = send_json(s, login_payload)
        
        # Kiểm tra xem login có thành công không
        if not resp or resp.get("status") != 200:
            print(">>> Đăng nhập thất bại! Dừng chương trình.")
            s.close()
            return

        # ---------------------------------------------------------
        # BƯỚC 4: TẠO PHÒNG (CREATE_ROOM)
        # ---------------------------------------------------------
        list_req = {
                "type": "LIST_ROOMS"
            }
        print("Sending LIST_ROOMS...")
        send_json(s, list_req)

        # Đóng kết nối
        s.close()

    except ConnectionRefusedError:
        print("Lỗi: Không thể kết nối đến server. Hãy chắc chắn server đang chạy!")
    except Exception as e:
        print(f"Lỗi không mong muốn: {e}")

if __name__ == "__main__":
    main()