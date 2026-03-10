// SPDX-License-Identifier: GPL-2.0
// TetrisinUEFI - Tetris running directly in UEFI firmware
// Copyright (C) 2026 SA.inc
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextIn.h>
#include <Library/IoLib.h>

// ============================================================
// TETRIS FOR UEFI
// "if you can boot it, you can play it"
// ============================================================

#define BOARD_W   10
#define BOARD_H   20
#define TICK_US   400000

#define RGB(r,g,b) ((UINT32)(((r)<<16)|((g)<<8)|(b)))
#define COL_BLACK  RGB(0,0,0)
#define COL_DGRAY  RGB(20,20,20)
#define COL_BORDER RGB(100,100,100)
#define COL_WHITE  RGB(255,255,255)
#define COL_SCORE  RGB(180,180,255)

#define COL_I  RGB(0,240,240)
#define COL_O  RGB(240,240,0)
#define COL_T  RGB(160,0,240)
#define COL_S  RGB(0,200,0)
#define COL_Z  RGB(220,0,0)
#define COL_J  RGB(0,0,220)
#define COL_L  RGB(240,140,0)

static const UINT32 PIECE_COLORS[7] = {
  COL_I, COL_O, COL_T, COL_S, COL_Z, COL_J, COL_L
};

static const UINT8 PIECES[7][4][16] = {
  // I
  { {0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0},
  {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0},
  {0,0,0,0, 0,0,0,0, 1,1,1,1, 0,0,0,0},
  {0,1,0,0, 0,1,0,0, 0,1,0,0, 0,1,0,0} },
  // O
  { {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0} },
  // T
  { {0,1,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,1,0,0, 0,1,1,0, 0,1,0,0, 0,0,0,0},
  {0,0,0,0, 1,1,1,0, 0,1,0,0, 0,0,0,0},
  {0,1,0,0, 1,1,0,0, 0,1,0,0, 0,0,0,0} },
  // S
  { {0,1,1,0, 1,1,0,0, 0,0,0,0, 0,0,0,0},
  {0,1,0,0, 0,1,1,0, 0,0,1,0, 0,0,0,0},
  {0,0,0,0, 0,1,1,0, 1,1,0,0, 0,0,0,0},
  {1,0,0,0, 1,1,0,0, 0,1,0,0, 0,0,0,0} },
  // Z
  { {1,1,0,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,0,1,0, 0,1,1,0, 0,1,0,0, 0,0,0,0},
  {0,0,0,0, 1,1,0,0, 0,1,1,0, 0,0,0,0},
  {0,1,0,0, 1,1,0,0, 1,0,0,0, 0,0,0,0} },
  // J
  { {1,0,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,1,1,0, 0,1,0,0, 0,1,0,0, 0,0,0,0},
  {0,0,0,0, 1,1,1,0, 0,0,1,0, 0,0,0,0},
  {0,1,0,0, 0,1,0,0, 1,1,0,0, 0,0,0,0} },
  // L
  { {0,0,1,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
  {0,1,0,0, 0,1,0,0, 0,1,1,0, 0,0,0,0},
  {0,0,0,0, 1,1,1,0, 1,0,0,0, 0,0,0,0},
  {1,1,0,0, 0,1,0,0, 0,1,0,0, 0,0,0,0} },
};

// --- Game state ---
static UINT8   Board[BOARD_H][BOARD_W];
static UINT8   LastDrawn[BOARD_H][BOARD_W];
static INT32   CurX, CurY, CurType, CurRot;
static INT32   NextType;
static UINT32  Score, Lines, Level;
static BOOLEAN GameOver;
static BOOLEAN NeedFullRedraw;

// --- GOP & layout ---
static EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
static UINT32 ScreenW, ScreenH;
static UINT32 BlockSize;
static UINT32 BoardX, BoardY;
static UINT32 UiX, UiY;

// --- RNG ---
static UINT32 RandState = 0xABCD1234;
static UINT32 Rand7(VOID) {
  RandState ^= RandState << 13;
  RandState ^= RandState >> 17;
  RandState ^= RandState << 5;
  return RandState % 7;
}


static VOID DrawRect(UINT32 X, UINT32 Y, UINT32 W, UINT32 H, UINT32 Color) {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Px;
  if (W == 0 || H == 0) return;
  if (X >= ScreenW || Y >= ScreenH) return;
  if (X + W > ScreenW) W = ScreenW - X;
  if (Y + H > ScreenH) H = ScreenH - Y;
  Px.Blue     = (UINT8)(Color & 0xFF);
  Px.Green    = (UINT8)((Color >> 8) & 0xFF);
  Px.Red      = (UINT8)((Color >> 16) & 0xFF);
  Px.Reserved = 0;
  Gop->Blt(Gop, &Px, EfiBltVideoFill, 0, 0, X, Y, W, H, 0);
}

static VOID DrawCell(INT32 Col, INT32 Row, UINT32 Color) {
  UINT32 X  = BoardX + (UINT32)Col * BlockSize + 1;
  UINT32 Y  = BoardY + (UINT32)Row * BlockSize + 1;
  UINT32 S  = BlockSize - 2;
  UINT32 Hi;
  DrawRect(X, Y, S, S, Color);
  if (Color != COL_DGRAY && Color != COL_BLACK && S > 3) {
    Hi = RGB(
      MIN(255, ((Color>>16)&0xFF) + 70),
             MIN(255, ((Color>>8)&0xFF)  + 70),
             MIN(255, (Color&0xFF)       + 70)
    );
    DrawRect(X, Y, S, 2, Hi);
    DrawRect(X, Y, 2, S, Hi);
  }
}

static const UINT8 FONT[10][5] = {
  {0x7,0x5,0x5,0x5,0x7},
  {0x2,0x6,0x2,0x2,0x7},
  {0x7,0x1,0x7,0x4,0x7},
  {0x7,0x1,0x7,0x1,0x7},
  {0x5,0x5,0x7,0x1,0x1},
  {0x7,0x4,0x7,0x1,0x7},
  {0x7,0x4,0x7,0x5,0x7},
  {0x7,0x1,0x1,0x1,0x1},
  {0x7,0x5,0x7,0x5,0x7},
  {0x7,0x5,0x7,0x1,0x7},
};

static VOID DrawNumber(UINT32 X, UINT32 Y, UINT32 Num, UINT32 Scale, UINT32 Color) {
  CHAR8 Buf[12];
  INT32 i, len, row, col, a, b;
  CHAR8 t;
  i = 0;
  if (Num == 0) {
    Buf[i++] = 0;
  } else {
    UINT32 n = Num;
    while (n > 0) { Buf[i++] = (CHAR8)(n % 10); n /= 10; }
    a = 0; b = i - 1;
    while (a < b) { t = Buf[a]; Buf[a] = Buf[b]; Buf[b] = t; a++; b--; }
  }
  len = i;
  for (i = 0; i < len; i++) {
    UINT32 dx = X + (UINT32)i * (Scale * 4);
    for (row = 0; row < 5; row++) {
      for (col = 0; col < 3; col++) {
        if (FONT[(UINT8)Buf[i]][row] & (UINT8)(1 << (2 - col))) {
          DrawRect(dx + (UINT32)col * Scale,
                   Y  + (UINT32)row * Scale,
                   Scale, Scale, Color);
        }
      }
    }
  }
}

//        N=13 O=14 P=15 Q=16 R=17 S=18 T=19 U=20 V=21 W=22 X=23 Y=24 Z=25
//        space=26  =27(equals)  /=28  dash=29
static const UINT8 LFONT[30][5] = {
  {0x2,0x5,0x7,0x5,0x5}, // A
  {0x6,0x5,0x6,0x5,0x6}, // B
  {0x3,0x4,0x4,0x4,0x3}, // C
  {0x6,0x5,0x5,0x5,0x6}, // D
  {0x7,0x4,0x6,0x4,0x7}, // E
  {0x7,0x4,0x6,0x4,0x4}, // F
  {0x3,0x4,0x5,0x5,0x3}, // G
  {0x5,0x5,0x7,0x5,0x5}, // H
  {0x7,0x2,0x2,0x2,0x7}, // I
  {0x1,0x1,0x1,0x5,0x2}, // J
  {0x5,0x5,0x6,0x5,0x5}, // K
  {0x4,0x4,0x4,0x4,0x7}, // L
  {0x5,0x7,0x5,0x5,0x5}, // M
  {0x5,0x7,0x7,0x5,0x5}, // N
  {0x2,0x5,0x5,0x5,0x2}, // O
  {0x6,0x5,0x6,0x4,0x4}, // P
  {0x2,0x5,0x5,0x7,0x3}, // Q
  {0x6,0x5,0x6,0x5,0x5}, // R
  {0x3,0x4,0x2,0x1,0x6}, // S
  {0x7,0x2,0x2,0x2,0x2}, // T
  {0x5,0x5,0x5,0x5,0x2}, // U
  {0x5,0x5,0x5,0x2,0x2}, // V
  {0x5,0x5,0x7,0x7,0x5}, // W
  {0x5,0x5,0x2,0x5,0x5}, // X
  {0x5,0x5,0x2,0x2,0x2}, // Y
  {0x7,0x1,0x2,0x4,0x7}, // Z
  {0x0,0x0,0x0,0x0,0x0}, // space
  {0x0,0x0,0x7,0x0,0x0}, // -
  {0x0,0x2,0x7,0x2,0x0}, // +
  {0x0,0x1,0x2,0x4,0x0}, // /
};

// Map ASCII to LFONT index
static INT32 CharToLFont(CHAR8 Ch) {
  if (Ch >= 'A' && Ch <= 'Z') return Ch - 'A';
  if (Ch >= 'a' && Ch <= 'z') return Ch - 'a';
  if (Ch == ' ') return 26;
  if (Ch == '-') return 27;
  if (Ch == '+') return 28;
  if (Ch == '/') return 29;
  return 26; // default space
}

static VOID DrawLabel(UINT32 X, UINT32 Y, const CHAR8 *Text, UINT32 Scale, UINT32 Color) {
  UINT32 i, dx;
  INT32  row, col, li;
  for (i = 0; Text[i] != 0; i++) {
    dx = X + i * (Scale * 4);
    li = CharToLFont(Text[i]);
    for (row = 0; row < 5; row++) {
      for (col = 0; col < 3; col++) {
        if (LFONT[li][row] & (UINT8)(1 << (2 - col))) {
          DrawRect(dx + (UINT32)col * Scale,
                   Y  + (UINT32)row * Scale,
                   Scale, Scale, Color);
        }
      }
    }
  }
}


static VOID ClearBoard(VOID) {
  ZeroMem(Board,     sizeof(Board));
  ZeroMem(LastDrawn, sizeof(LastDrawn));
  Score = 0; Lines = 0; Level = 1;
  GameOver = FALSE;
  NeedFullRedraw = TRUE;
}

static BOOLEAN PieceCollides(INT32 Type, INT32 Rot, INT32 X, INT32 Y) {
  INT32 r, c;
  for (r = 0; r < 4; r++) {
    for (c = 0; c < 4; c++) {
      if (PIECES[Type][Rot][r*4+c]) {
        INT32 nx = X + c;
        INT32 ny = Y + r;
        if (nx < 0 || nx >= BOARD_W || ny >= BOARD_H) return TRUE;
        if (ny >= 0 && Board[ny][nx]) return TRUE;
      }
    }
  }
  return FALSE;
}

static VOID LockPiece(VOID) {
  INT32 r, c;
  for (r = 0; r < 4; r++) {
    for (c = 0; c < 4; c++) {
      if (PIECES[CurType][CurRot][r*4+c]) {
        INT32 ny = CurY + r;
        INT32 nx = CurX + c;
        if (ny >= 0 && ny < BOARD_H && nx >= 0 && nx < BOARD_W)
          Board[ny][nx] = (UINT8)(CurType + 1);
      }
    }
  }
}

static UINT32 ClearLines(VOID) {
  INT32 r, c, w;
  UINT32 cleared = 0;
  for (r = BOARD_H - 1; r >= 0; r--) {
    BOOLEAN full = TRUE;
    for (c = 0; c < BOARD_W; c++) {
      if (!Board[r][c]) { full = FALSE; break; }
    }
    if (full) {
      cleared++;
      for (w = r; w > 0; w--)
        CopyMem(Board[w], Board[w-1], BOARD_W);
      ZeroMem(Board[0], BOARD_W);
      r++;
    }
  }
  if (cleared > 0) NeedFullRedraw = TRUE;
  return cleared;
}

static VOID SpawnPiece(VOID) {
  CurType  = NextType;
  CurRot   = 0;
  CurX     = BOARD_W / 2 - 2;
  CurY     = 0;
  NextType = (INT32)Rand7();
  if (PieceCollides(CurType, CurRot, CurX, CurY))
    GameOver = TRUE;
}


static VOID RenderInit(VOID) {
  INT32 r, c;
  DrawRect(0, 0, ScreenW, ScreenH, COL_BLACK);
  DrawRect(BoardX - 2, BoardY - 2,
           BOARD_W * BlockSize + 4,
           BOARD_H * BlockSize + 4,
           COL_BORDER);
  for (r = 0; r < BOARD_H; r++) {
    for (c = 0; c < BOARD_W; c++) {
      DrawCell(c, r, COL_DGRAY);
    }
  }
  ZeroMem(LastDrawn, sizeof(LastDrawn));
  NeedFullRedraw = FALSE;
}

static VOID BuildFrame(UINT8 Frame[BOARD_H][BOARD_W]) {
  INT32 r, c;
  CopyMem(Frame, Board, sizeof(Board));
  if (!GameOver) {
    for (r = 0; r < 4; r++) {
      for (c = 0; c < 4; c++) {
        if (PIECES[CurType][CurRot][r*4+c]) {
          INT32 fr = CurY + r;
          INT32 fc = CurX + c;
          if (fr >= 0 && fr < BOARD_H && fc >= 0 && fc < BOARD_W)
            Frame[fr][fc] = (UINT8)(CurType + 0x10);
        }
      }
    }
  }
}

static VOID RenderFrame(VOID) {
  UINT8  Frame[BOARD_H][BOARD_W];
  INT32  r, c;
  UINT32 Color;
  UINT8  v;

  if (NeedFullRedraw) RenderInit();

  BuildFrame(Frame);

  for (r = 0; r < BOARD_H; r++) {
    for (c = 0; c < BOARD_W; c++) {
      v = Frame[r][c];
      if (v == LastDrawn[r][c]) continue;
      if (v == 0) {
        Color = COL_DGRAY;
      } else if (v >= 0x10) {
        Color = PIECE_COLORS[v - 0x10];
      } else {
        Color = PIECE_COLORS[v - 1];
      }
      DrawCell(c, r, Color);
      LastDrawn[r][c] = v;
    }
  }
}

static VOID RenderUI(VOID) {
  UINT32 Scale = BlockSize / 6;
  UINT32 Gap;
  UINT32 hy;
  INT32  r, c;
  UINT32 px, py;

  if (Scale < 2) Scale = 2;
  Gap = Scale * 8;

  DrawRect(UiX, UiY, BlockSize * 5, BOARD_H * BlockSize, COL_BLACK);

  // Score / Lines / Level at top
  DrawNumber(UiX, UiY,         Score, Scale, COL_SCORE);
  DrawNumber(UiX, UiY + Gap,   Lines, Scale, COL_WHITE);
  DrawNumber(UiX, UiY + Gap*2, Level, Scale, RGB(255,180,0));

  px = UiX;
  py = UiY + Gap * 3 + 4;
  DrawRect(px - 2, py - 2, 4*BlockSize+4, 4*BlockSize+4, COL_BORDER);
  DrawRect(px, py, 4*BlockSize, 4*BlockSize, COL_DGRAY);
  for (r = 0; r < 4; r++) {
    for (c = 0; c < 4; c++) {
      if (PIECES[NextType][0][r*4+c]) {
        DrawRect(px + (UINT32)c*BlockSize + 1,
                 py + (UINT32)r*BlockSize + 1,
                 BlockSize - 2, BlockSize - 2,
                 PIECE_COLORS[NextType]);
      }
    }
  }

  {
    UINT32 hhs = 2;
    UINT32 hls = hhs * 7;
    hy = py + 4*BlockSize + hhs*4;
    DrawLabel(px, hy,          "W-A-S-D",  hhs, RGB(160,160,255));
    DrawLabel(px, hy + hls,    "W  ROTATE", hhs, RGB(200,200,200));
    DrawLabel(px, hy + hls*2,  "S  DOWN",   hhs, RGB(200,200,200));
    DrawLabel(px, hy + hls*3,  "A  LEFT",   hhs, RGB(200,200,200));
    DrawLabel(px, hy + hls*4,  "D  RIGHT",  hhs, RGB(200,200,200));
    DrawLabel(px, hy + hls*5,  "SPC DROP",  hhs, RGB(255,220,0));
    DrawLabel(px, hy + hls*6,  "R  RESET",  hhs, RGB(255,100,100));
    DrawLabel(px, hy + hls*7,  "Q  QUIT",   hhs, RGB(255,100,100));
    DrawLabel(px, hy + hls*8,  "M  MUTE",   hhs, RGB(100,255,100));
  }
}

static VOID RenderGameOver(VOID) {
  UINT32 Scale = BlockSize / 4;
  if (Scale < 2) Scale = 2;
  DrawRect(BoardX, BoardY,
           BOARD_W * BlockSize, BOARD_H * BlockSize, RGB(0,0,0));
  DrawNumber(BoardX + BlockSize, BoardY + BOARD_H * BlockSize / 2,
             Score, Scale, RGB(255,60,60));
}



// ============================================================
// PC Speaker - Korobeiniki (Tetris Theme)
// ============================================================

static BOOLEAN Muted = FALSE;

static VOID SpeakerOn(UINT32 FreqHz) {
  if (Muted) return;
  UINT32 Divisor;
  UINT8  Val;
  if (FreqHz == 0) return;
  Divisor = 1193180 / FreqHz;
  IoWrite8(0x43, 0xB6);
  IoWrite8(0x42, (UINT8)(Divisor & 0xFF));
  IoWrite8(0x42, (UINT8)((Divisor >> 8) & 0xFF));
  Val = IoRead8(0x61);
  IoWrite8(0x61, Val | 0x03);
}

static VOID SpeakerOff(VOID) {
  UINT8 Val = IoRead8(0x61);
  IoWrite8(0x61, Val & 0xFC);
}

// Note frequencies (Hz)
#define E5  659
#define B4  494
#define C5  523
#define D5  587
#define A4  440
#define A5  880
#define G4  392
#define F4  349
#define F5  698
#define G5  784
#define R   0

// Korobeiniki - {freq, duration in ms}
static const UINT32 MELODY[][2] = {
  {E5,300},{B4,150},{C5,150},{D5,300},{C5,150},{B4,150},
  {A4,300},{A4,150},{C5,150},{E5,300},{D5,150},{C5,150},
  {B4,300},{B4,150},{C5,150},{D5,300},{E5,300},
  {C5,300},{A4,300},{A4,300},{R,150},
  {D5,300},{D5,150},{F5,150},{A5,300},{G5,150},{F5,150},
  {E5,300},{C5,150},{E5,300},{D5,150},{C5,150},
  {B4,300},{B4,150},{C5,150},{D5,300},{E5,300},
  {C5,300},{A4,300},{A4,300},{R,150},
  {E5,300},{C5,300},{D5,300},{B4,300},
  {C5,300},{A4,300},{G4,300},{R,150},
  {E5,300},{C5,300},{D5,300},{B4,300},
  {C5,150},{E5,150},{A5,300},{G5,150},{F5,150},
  {E5,300},{C5,150},{E5,300},{D5,150},{C5,150},
  {B4,300},{B4,150},{C5,150},{D5,300},{E5,300},
  {C5,300},{A4,300},{A4,300},{R,0},
};

#define MELODY_COUNT (sizeof(MELODY) / sizeof(MELODY[0]))

static UINT32  MelodyPos     = 0;
static UINT32  NoteTimeLeft  = 0; // microseconds remaining on current note
static BOOLEAN NoteGap       = FALSE;
#define NOTE_GAP_US 30000

static VOID TickMusic(UINT32 ElapsedUs) {
  if (NoteGap) {
    if (ElapsedUs >= NoteTimeLeft) {
      NoteTimeLeft = 0;
      NoteGap = FALSE;
    } else {
      NoteTimeLeft -= ElapsedUs;
      return;
    }
  }

  if (NoteTimeLeft == 0) {
    // Advance to next note
    if (MELODY[MelodyPos][1] == 0) MelodyPos = 0;
    if (MELODY[MelodyPos][0] == R) {
      SpeakerOff();
    } else {
      SpeakerOn(MELODY[MelodyPos][0]);
    }
    NoteTimeLeft = MELODY[MelodyPos][1] * 1000; // ms to us
    MelodyPos++;
    if (MelodyPos >= MELODY_COUNT) MelodyPos = 0;
  } else {
    if (ElapsedUs >= NoteTimeLeft) {
      SpeakerOff();
      NoteTimeLeft = 0;
      NoteGap = TRUE;
      NoteTimeLeft = NOTE_GAP_US;
    } else {
      NoteTimeLeft -= ElapsedUs;
    }
  }
}

static BOOLEAN ReadKey(EFI_INPUT_KEY *Key) {
  return !EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, Key));
}


EFI_STATUS
EFIAPI
UefiMain(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  EFI_STATUS    Status;
  EFI_INPUT_KEY Key;
  UINT64        TickAccum;
  UINT32        TickRate;
  UINT32        BoardPixH;
  UINT32        BoardPixW;
  UINT32        TotalW;
  BOOLEAN       UiDirty;

  Status = gBS->LocateProtocol(
    &gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&Gop);
  if (EFI_ERROR(Status)) {
    Print(L"No GOP!\n");
    return Status;
  }

  ScreenW = Gop->Mode->Info->HorizontalResolution;
  ScreenH = Gop->Mode->Info->VerticalResolution;

  // Block size: fit board into 80% of screen height
  BlockSize = (ScreenH * 80 / 100) / BOARD_H;
  if (BlockSize > 40) BlockSize = 40;
  if (BlockSize < 8)  BlockSize = 8;

  BoardPixH = BOARD_H * BlockSize;
  BoardPixW = BOARD_W * BlockSize;
  TotalW    = BoardPixW + BlockSize * 5;

  BoardX = (TotalW < ScreenW) ? (ScreenW - TotalW) / 2 : 4;
  BoardY = (BoardPixH < ScreenH) ? (ScreenH - BoardPixH) / 2 : 4;
  UiX    = BoardX + BoardPixW + BlockSize;
  UiY    = BoardY;

  RandState = (UINT32)(ScreenW * 31337 + ScreenH * 13337 + 0xBEEF);

  gST->ConOut->EnableCursor(gST->ConOut, FALSE);

  restart:
  MelodyPos = 0;
  NoteTimeLeft = 0;
  NoteGap = FALSE;
  SpeakerOff();
  ClearBoard();
  NextType   = (INT32)Rand7();
  SpawnPiece();
  NeedFullRedraw = TRUE;
  TickAccum  = 0;
  UiDirty    = TRUE;

  while (1) {
    while (ReadKey(&Key)) {
      if (Key.ScanCode == SCAN_LEFT ||
        Key.UnicodeChar == L'a' || Key.UnicodeChar == L'A') {
        if (!PieceCollides(CurType, CurRot, CurX-1, CurY)) CurX--;
        } else if (Key.ScanCode == SCAN_RIGHT ||
          Key.UnicodeChar == L'd' || Key.UnicodeChar == L'D') {
          if (!PieceCollides(CurType, CurRot, CurX+1, CurY)) CurX++;
          } else if (Key.ScanCode == SCAN_DOWN ||
            Key.UnicodeChar == L's' || Key.UnicodeChar == L'S') {
            if (!PieceCollides(CurType, CurRot, CurX, CurY+1)) CurY++;
            } else if (Key.ScanCode == SCAN_UP ||
              Key.UnicodeChar == L'w' || Key.UnicodeChar == L'W') {
              INT32 nr = (CurRot + 1) % 4;
            if (!PieceCollides(CurType, nr, CurX, CurY)) CurRot = nr;
              } else if (Key.UnicodeChar == L' ') {
                while (!PieceCollides(CurType, CurRot, CurX, CurY+1)) CurY++;
                TickAccum = 0xFFFFFFFFFFFFFFFFULL;
              } else if (Key.UnicodeChar == L'r' || Key.UnicodeChar == L'R') {
                goto restart;
              } else if (Key.UnicodeChar == L'q' || Key.UnicodeChar == L'Q') {
                SpeakerOff();
                DrawRect(0, 0, ScreenW, ScreenH, COL_BLACK);
                gST->ConOut->EnableCursor(gST->ConOut, TRUE);
                return EFI_SUCCESS;
              }
              UiDirty = TRUE;
    }

    TickRate = TICK_US - (Level > 13 ? 13 : Level - 1) * 28000;
    if (TickRate < 60000) TickRate = 60000;

    TickAccum += TickRate / 20;
    if (TickAccum >= (UINT64)TickRate) {
      TickAccum = 0;
      if (!PieceCollides(CurType, CurRot, CurX, CurY+1)) {
        CurY++;
      } else {
        UINT32 cleared;
        LockPiece();
        cleared = ClearLines();
        if (cleared > 0) {
          static const UINT32 ST[5] = {0,100,300,500,800};
          Score += ST[cleared > 4 ? 4 : cleared] * Level;
          Lines += cleared;
          Level  = Lines / 10 + 1;
        }
        SpawnPiece();
        UiDirty = TRUE;
        if (GameOver) break;
      }
    }

    RenderFrame();
    if (UiDirty) {
      RenderUI();
      UiDirty = FALSE;
    }

    // Fixed 10ms slices - music and input independent of game speed
    {
      UINT32 SliceUs = 10000;
      UINT32 Total   = 20000; // always 20ms per frame regardless of TickRate
      UINT32 Done    = 0;
      while (Done < Total) {
        UINT32 Step = (Done + SliceUs > Total) ? (Total - Done) : SliceUs;
        gBS->Stall(Step);
        TickMusic(Step);
        // Poll input mid-frame so it never feels laggy
        while (ReadKey(&Key)) {
          if (Key.ScanCode == SCAN_LEFT ||
            Key.UnicodeChar == L'a' || Key.UnicodeChar == L'A') {
            if (!PieceCollides(CurType, CurRot, CurX-1, CurY)) CurX--;
            } else if (Key.ScanCode == SCAN_RIGHT ||
              Key.UnicodeChar == L'd' || Key.UnicodeChar == L'D') {
              if (!PieceCollides(CurType, CurRot, CurX+1, CurY)) CurX++;
              } else if (Key.ScanCode == SCAN_DOWN ||
                Key.UnicodeChar == L's' || Key.UnicodeChar == L'S') {
                if (!PieceCollides(CurType, CurRot, CurX, CurY+1)) CurY++;
                } else if (Key.ScanCode == SCAN_UP ||
                  Key.UnicodeChar == L'w' || Key.UnicodeChar == L'W') {
                  INT32 nr = (CurRot + 1) % 4;
                if (!PieceCollides(CurType, nr, CurX, CurY)) CurRot = nr;
                  } else if (Key.UnicodeChar == L' ') {
                    while (!PieceCollides(CurType, CurRot, CurX, CurY+1)) CurY++;
                    TickAccum = 0xFFFFFFFFFFFFFFFFULL;
                  } else if (Key.UnicodeChar == L'r' || Key.UnicodeChar == L'R') {
                    goto restart;
                  } else if (Key.UnicodeChar == L'm' || Key.UnicodeChar == L'M') {
                    Muted = !Muted;
                    if (Muted) SpeakerOff();
                  } else if (Key.UnicodeChar == L'q' || Key.UnicodeChar == L'Q') {
                    SpeakerOff();
                    DrawRect(0, 0, ScreenW, ScreenH, COL_BLACK);
                    gST->ConOut->EnableCursor(gST->ConOut, TRUE);
                    return EFI_SUCCESS;
                  }
                  UiDirty = TRUE;
                  RenderFrame();
                  if (UiDirty) { RenderUI(); UiDirty = FALSE; }
        }
        Done += Step;
      }
    }

  }

  SpeakerOff();
  RenderGameOver();
  while (1) {
    if (ReadKey(&Key)) {
      if (Key.UnicodeChar == L'r' || Key.UnicodeChar == L'R') goto restart;
      if (Key.UnicodeChar == L'q' || Key.UnicodeChar == L'Q') break;
    }
    TickMusic(50000);
    gBS->Stall(50000);
  }

  SpeakerOff();
  DrawRect(0, 0, ScreenW, ScreenH, COL_BLACK);
  gST->ConOut->EnableCursor(gST->ConOut, TRUE);
  return EFI_SUCCESS;
}
