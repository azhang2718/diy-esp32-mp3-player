#pragma once

// 16x16 Claude-Pikachu running animation frames
// U8g2 bitmap format: column-major, LSB at top
// Each frame: 16 columns × 2 bytes per column = 32 bytes

// Frame 0: Running, right leg forward
const unsigned char CLAUDE_FRAME_0[32] = {
    0x00, 0x00,  // Col 0
    0x00, 0x00,  // Col 1
    0x00, 0x0C,  // Col 2 - ears
    0x00, 0x1E,  // Col 3
    0x00, 0x3F,  // Col 4 - head
    0x80, 0x7F,  // Col 5
    0xC0, 0x7E,  // Col 6
    0xE0, 0x3C,  // Col 7 - ears & head
    0xE0, 0x18,  // Col 8 - face
    0xE0, 0x38,  // Col 9
    0xE0, 0x78,  // Col 10 - body
    0x60, 0xF0,  // Col 11
    0x60, 0xE0,  // Col 12 - legs right forward
    0x30, 0xC0,  // Col 13
    0x00, 0x00,  // Col 14
    0x00, 0x00   // Col 15
};

// Frame 1: Running, left leg forward (mirror pose)
const unsigned char CLAUDE_FRAME_1[32] = {
    0x00, 0x00,  // Col 0
    0x00, 0x00,  // Col 1
    0x30, 0x00,  // Col 2 - ears
    0x78, 0x00,  // Col 3
    0xFC, 0x00,  // Col 4 - head
    0xFE, 0x01,  // Col 5
    0x7E, 0x03,  // Col 6
    0x3C, 0x07,  // Col 7 - ears & head
    0x18, 0x07,  // Col 8 - face
    0x1C, 0x07,  // Col 9
    0x1E, 0x07,  // Col 10 - body
    0x0F, 0x06,  // Col 11
    0x07, 0x06,  // Col 12 - legs left forward
    0x03, 0x0C,  // Col 13
    0x00, 0x00,  // Col 14
    0x00, 0x00   // Col 15
};

// Frame 2: Mid-run (neutral stance)
const unsigned char CLAUDE_FRAME_2[32] = {
    0x00, 0x00,  // Col 0
    0x00, 0x0C,  // Col 1 - ears
    0x00, 0x1E,  // Col 2
    0x00, 0x3F,  // Col 3 - head
    0x80, 0x7F,  // Col 4
    0xC0, 0x7E,  // Col 5
    0xE0, 0x3C,  // Col 6 - face
    0xE0, 0x18,  // Col 7
    0xE0, 0x38,  // Col 8 - body
    0xE0, 0x78,  // Col 9
    0x60, 0xF0,  // Col 10 - legs
    0x30, 0xE0,  // Col 11
    0x18, 0xC0,  // Col 12 - stance
    0x00, 0x00,  // Col 13
    0x00, 0x00,  // Col 14
    0x00, 0x00   // Col 15
};

const unsigned char* CLAUDE_FRAMES[] = {
    CLAUDE_FRAME_0,
    CLAUDE_FRAME_1,
    CLAUDE_FRAME_2
};

const int CLAUDE_FRAME_COUNT = 3;
