Đây là script để tạo database
-- 1. Tắt kiểm tra khóa ngoại để xóa bảng dễ dàng hơn
SET FOREIGN_KEY_CHECKS = 0;

-- 2. Xóa các bảng cũ (Nếu tồn tại)
DROP TABLE IF EXISTS room_participants;
DROP TABLE IF EXISTS room_questions; 
DROP TABLE IF EXISTS rooms;
DROP TABLE IF EXISTS practice_history;
DROP TABLE IF EXISTS questions;
DROP TABLE IF EXISTS users;

-- 3. Bật lại kiểm tra khóa ngoại
SET FOREIGN_KEY_CHECKS = 1;

-- =============================================
-- 4. TẠO CÁC BẢNG MỚI
-- =============================================

-- Bảng USERS
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    status INT DEFAULT 1 COMMENT '1: Active, 0: Banned',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Bảng QUESTIONS (Lưu nội dung dạng JSON Object)
CREATE TABLE questions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    content JSON NOT NULL COMMENT 'Chứa question_text, options, correct_answer',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Bảng ROOMS (Lưu danh sách câu hỏi dạng JSON Map)
CREATE TABLE rooms (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_name VARCHAR(100) NOT NULL,
    host_id INT NOT NULL,
    questions_data JSON NOT NULL COMMENT 'Snapshot câu hỏi: {"101": {...}, "102": {...}}',
    num_questions INT NOT NULL,
    duration_minutes INT NOT NULL,
    start_time TIMESTAMP NULL,
    status INT DEFAULT 0 COMMENT '0: Waiting, 1: Running, 2: Finished',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (host_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Bảng ROOM_PARTICIPANTS (Lưu bài làm dạng JSON Map)
CREATE TABLE room_participants (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id INT NOT NULL,
    user_id INT NOT NULL,
    user_answers JSON DEFAULT NULL COMMENT 'Bài làm: {"101": "A", "102": "C"}',
    score INT DEFAULT 0,
    is_late TINYINT DEFAULT 0,
    status INT DEFAULT 0 COMMENT '0: Joined, 1: Submitted',
    finished_at TIMESTAMP NULL,
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Bảng PRACTICE_HISTORY (Lưu lịch sử luyện tập cá nhân)
CREATE TABLE practice_history (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    questions_data JSON NOT NULL COMMENT 'Snapshot câu hỏi',
    user_answers JSON NOT NULL COMMENT 'Bài làm của user',
    num_questions INT NOT NULL,
    duration_minutes INT NOT NULL,
    score INT NOT NULL,
    practiced_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

