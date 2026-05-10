#pragma once

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Color {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;
};

struct Camera3D {
    Vector3 position = {};
    Vector3 target = {};
    Vector3 up = {};
    float fovy = 0.0f;
    int projection = 0;
};

inline constexpr int CAMERA_PERSPECTIVE = 0;
inline constexpr int FLAG_MSAA_4X_HINT = 0;
inline constexpr int FLAG_WINDOW_RESIZABLE = 0;
inline constexpr int MOUSE_BUTTON_RIGHT = 1;

inline constexpr int KEY_W = 87;
inline constexpr int KEY_S = 83;
inline constexpr int KEY_A = 65;
inline constexpr int KEY_D = 68;
inline constexpr int KEY_Q = 81;
inline constexpr int KEY_E = 69;
inline constexpr int KEY_R = 82;
inline constexpr int KEY_F = 70;
inline constexpr int KEY_C = 67;
inline constexpr int KEY_Z = 90;
inline constexpr int KEY_X = 88;
inline constexpr int KEY_TAB = 258;
inline constexpr int KEY_LEFT_SHIFT = 340;
inline constexpr int KEY_LEFT_CONTROL = 341;
inline constexpr int KEY_RIGHT_SHIFT = 344;
inline constexpr int KEY_RIGHT_CONTROL = 345;
inline constexpr int KEY_RIGHT = 262;
inline constexpr int KEY_LEFT = 263;
inline constexpr int KEY_DOWN = 264;
inline constexpr int KEY_UP = 265;
inline constexpr int KEY_EQUAL = 61;
inline constexpr int KEY_MINUS = 45;
inline constexpr int KEY_ONE = 49;
inline constexpr int KEY_TWO = 50;
inline constexpr int KEY_THREE = 51;
inline constexpr int KEY_FOUR = 52;
inline constexpr int KEY_FIVE = 53;
inline constexpr int KEY_SIX = 54;
inline constexpr int KEY_KP_ADD = 334;
inline constexpr int KEY_KP_SUBTRACT = 333;

inline constexpr int GAMEPAD_AXIS_LEFT_X = 0;
inline constexpr int GAMEPAD_AXIS_LEFT_Y = 1;
inline constexpr int GAMEPAD_AXIS_RIGHT_X = 2;
inline constexpr int GAMEPAD_AXIS_RIGHT_Y = 3;
inline constexpr int GAMEPAD_AXIS_LEFT_TRIGGER = 4;
inline constexpr int GAMEPAD_AXIS_RIGHT_TRIGGER = 5;

inline constexpr int GAMEPAD_BUTTON_LEFT_FACE_UP = 1;
inline constexpr int GAMEPAD_BUTTON_LEFT_FACE_RIGHT = 2;
inline constexpr int GAMEPAD_BUTTON_LEFT_FACE_DOWN = 3;
inline constexpr int GAMEPAD_BUTTON_LEFT_FACE_LEFT = 4;
inline constexpr int GAMEPAD_BUTTON_RIGHT_FACE_UP = 5;
inline constexpr int GAMEPAD_BUTTON_RIGHT_FACE_RIGHT = 6;
inline constexpr int GAMEPAD_BUTTON_RIGHT_FACE_DOWN = 7;
inline constexpr int GAMEPAD_BUTTON_RIGHT_FACE_LEFT = 8;
inline constexpr int GAMEPAD_BUTTON_LEFT_TRIGGER_1 = 9;
inline constexpr int GAMEPAD_BUTTON_RIGHT_TRIGGER_1 = 11;
inline constexpr int GAMEPAD_BUTTON_LEFT_THUMB = 15;
inline constexpr int GAMEPAD_BUTTON_RIGHT_THUMB = 16;

inline constexpr Color BLACK = {0, 0, 0, 255};
inline constexpr Color RED = {230, 41, 55, 255};
inline constexpr Color YELLOW = {253, 249, 0, 255};

inline void SetConfigFlags(unsigned int) {}
inline void InitWindow(int, int, const char*) {}
inline void SetTargetFPS(int) {}
inline bool WindowShouldClose() { return false; }
inline float GetFrameTime() { return 1.0f / 60.0f; }
inline Vector2 GetMouseDelta() { return {}; }
inline float GetMouseWheelMove() { return 0.0f; }
inline bool IsMouseButtonDown(int) { return false; }
inline bool IsKeyDown(int) { return false; }
inline bool IsKeyPressed(int) { return false; }
inline bool IsKeyPressedRepeat(int) { return false; }
inline int GetCharPressed() { return 0; }
inline bool IsGamepadAvailable(int) { return false; }
inline float GetGamepadAxisMovement(int, int) { return 0.0f; }
inline bool IsGamepadButtonPressed(int, int) { return false; }
inline bool IsGamepadButtonDown(int, int) { return false; }
inline const char* GetGamepadName(int) { return nullptr; }
inline double GetTime() { return 0.0; }
inline void BeginDrawing() {}
inline void EndDrawing() {}
inline void ClearBackground(Color) {}
inline void BeginMode3D(Camera3D) {}
inline void EndMode3D() {}
inline void CloseWindow() {}
