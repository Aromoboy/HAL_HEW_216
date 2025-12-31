
#ifndef __CONIOEX_H
#define __CONIOEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <process.h>
#include <windows.h>
#include <tchar.h>
#include <mmsystem.h>
#include <digitalv.h>
#pragma comment(lib, "winmm.lib")

#ifdef CONIOEX
#define CONIOEX_INST extern
#else 
#define CONIOEX_INST
#endif 

#ifdef __BORLANDC__
#define NOCURSOR		_NOCURSOR
#define SOLIDCURSOR		_SOLIDCURSOR
#define NORMALCURSOR	_NORMALCURSOR
__inline void setcursortype(int cur_t) {_setcursortype(cur_t);}
__inline int _kbhit(void) {return kbhit();}
#else 

#pragma warning(disable:4996)

#define NOCURSOR		0
#define SOLIDCURSOR		1
#define NORMALCURSOR	2

#define _NOCURSOR		NOCURSOR
#define _SOLIDCURSOR	SOLIDCURSOR
#define _NORMALCURSOR	NORMALCURSOR

#ifndef __COLORS
#define __COLORS
enum COLORS {
	BLACK,			
	BLUE,			
	GREEN,			
	CYAN,			
	RED,			
	MAGENTA,		
	BROWN,			
	LIGHTGRAY,		
	DARKGRAY,		
	LIGHTBLUE,		
	LIGHTGREEN,		
	LIGHTCYAN,		
	LIGHTRED,		
	LIGHTMAGENTA,	
	YELLOW,			
	WHITE			
};
#endif 

CONIOEX_INST WORD	__conioex_h_wAttribute		
#ifndef CONIOEX
= LIGHTGRAY
#endif 
;




int wherex(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return csbi.dwCursorPosition.X - csbi.srWindow.Left + 1;
}
#endif 


int wherey(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return csbi.dwCursorPosition.Y - csbi.srWindow.Top + 1;
}
#endif 


void gotoxy(int x, int y)
#ifdef CONIOEX
;
#else 
{
	COORD c;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	c.X = x - 1;
	c.Y = y - 1;
	if (GetConsoleScreenBufferInfo(h, &csbi))
		c.Y += csbi.srWindow.Top;
	SetConsoleCursorPosition(h, c);
}
#endif 


void clreol(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	COORD	c;
	HANDLE	h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(h, &csbi)) {
		DWORD num;

		c = csbi.dwCursorPosition;
		FillConsoleOutputAttribute(h, csbi.wAttributes,
			csbi.srWindow.Right - csbi.dwCursorPosition.X + 1, c, &num);
		FillConsoleOutputCharacter(h, ' ',
			csbi.srWindow.Right - csbi.dwCursorPosition.X + 1, c, &num);
	}
}
#endif 


void clrscr(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD num;
	COORD c = {0, 0};
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(h, &csbi)) {
		__conioex_h_wAttribute = csbi.wAttributes;
		FillConsoleOutputAttribute(h, csbi.wAttributes, csbi.dwSize.X * csbi.dwSize.Y, c, &num);
		FillConsoleOutputCharacter(h, TEXT(' '), csbi.dwSize.X * csbi.dwSize.Y, c, &num);
		gotoxy(csbi.srWindow.Left + 1, csbi.srWindow.Top + 1);
	}
}
#endif 


void highvideo(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(h, &csbi))
		SetConsoleTextAttribute(h, csbi.wAttributes | FOREGROUND_INTENSITY);
}
#endif 


void lowvideo(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(h, &csbi))
		SetConsoleTextAttribute(h, csbi.wAttributes & ~FOREGROUND_INTENSITY);
}
#endif 


void normvideo(void)
#ifdef CONIOEX
;
#else 
{
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), LIGHTGRAY);
}
#endif 

void textbackground(int newcolor)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleScreenBufferInfo(h, &csbi))
		SetConsoleTextAttribute(h, (csbi.wAttributes & ~0x00f0) | ((newcolor & 0x07) << 4));
}
#endif 

void textcolor(int newcolor)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleScreenBufferInfo(h, &csbi))
		SetConsoleTextAttribute(h, (csbi.wAttributes & ~0x0f) | (newcolor & 0x0f));
}
#endif 

void textattr(int newattr)
#ifdef CONIOEX
;
#else 
{
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), newattr);
}
#endif 

void setcursortype(int cur_t)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_CURSOR_INFO	cci;
	static	int nSize = -1;

	if (nSize < 0) {
		if (GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cci))
			nSize = (int)cci.dwSize;
		else
			nSize = 25;
	}
	cci.dwSize		= (cur_t < NORMALCURSOR) ? 100 : nSize;
	cci.bVisible	= (cur_t != NOCURSOR);
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cci);
}
#endif 
__inline void _setcursortype(int cur_t) {setcursortype(cur_t);}


void insline(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(h, &csbi)) {
		COORD	c;
		DWORD	len;
		DWORD	num;
		LPTSTR	psz;
		LPWORD	pw;

		c.X = csbi.srWindow.Left;
		len = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		psz = (LPTSTR)_alloca(len * sizeof(TCHAR));
		pw = (LPWORD)_alloca(len * sizeof(WORD));
		for (c.Y = csbi.srWindow.Bottom; c.Y > csbi.dwCursorPosition.Y; c.Y--) {
			c.Y--;
			ReadConsoleOutputAttribute(h, pw, len, c, &num);
			ReadConsoleOutputCharacter(h, psz, len, c, &num);
			c.Y++;
			WriteConsoleOutputAttribute(h, pw, len, c, &num);
			WriteConsoleOutputCharacter(h, psz, len, c, &num);
		}
		FillConsoleOutputAttribute(h, csbi.wAttributes, len, c, &num);
		FillConsoleOutputCharacter(h, TEXT(' '), len, c, &num);
	}
}
#endif 


void delline(void)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(h, &csbi)) {
		DWORD	num;
		COORD	c;
		DWORD	len;
		LPTSTR	psz;
		LPWORD	pw;

		c.X = csbi.srWindow.Left;
		len = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		psz = (LPTSTR)_alloca(len * sizeof(TCHAR));
		pw = (LPWORD)_alloca(len * sizeof(WORD));
		for (c.Y = csbi.dwCursorPosition.Y; c.Y < csbi.srWindow.Bottom; c.Y++) {
			c.Y++;
			ReadConsoleOutputAttribute(h, pw, len, c, &num);
			ReadConsoleOutputCharacter(h, psz, len, c, &num);
			c.Y--;
			WriteConsoleOutputAttribute(h, pw, len, c, &num);
			WriteConsoleOutputCharacter(h, psz, len, c, &num);
		}
		FillConsoleOutputAttribute(h, __conioex_h_wAttribute, len, c, &num);
		FillConsoleOutputCharacter(h, TEXT(' '), len, c, &num);
	}
}
#endif 


void width(int w, int h)
#ifdef CONIOEX
;
#else 
{
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hStdOut, &csbi);
	int oldr = csbi.dwSize.Y;
	int oldc = csbi.dwSize.X;
	int cols = w;
	int rows = h;
	if (cols == 0) cols = oldc;
	if (rows == 0) rows = oldr;
	COORD newSize = { (short)cols, (short)rows };
	SMALL_RECT rect = { 0, 0, (short)(cols - 1), (short)(rows - 1) };

	if (oldr <= rows) {
		if (oldc <= cols) {			
			
		} else {					
			SMALL_RECT tmp = { 0, 0, (short)(cols - 1), (short)(oldr - 1) };
			SetConsoleWindowInfo(hStdOut, TRUE, &tmp);
		}
	} else {
		if (oldc <= cols) {			
			SMALL_RECT tmp = { 0, 0, (short)(oldc - 1), (short)(rows - 1) };
			SetConsoleWindowInfo(hStdOut, TRUE, &tmp);
		} else {					
			SetConsoleWindowInfo(hStdOut, TRUE, &rect);
			SetConsoleScreenBufferSize(hStdOut, newSize);
			GetConsoleScreenBufferInfo(hStdOut, &csbi);
			return;
		}
	}
	SetConsoleScreenBufferSize(hStdOut, newSize);
	SetConsoleWindowInfo(hStdOut, TRUE, &rect);
	GetConsoleScreenBufferInfo(hStdOut, &csbi);
}
#endif 


int fontsize(int h)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_FONT_INFOEX cfiEx;
	COORD size;
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	ZeroMemory(&cfiEx, sizeof(cfiEx));
	cfiEx.cbSize = sizeof(cfiEx);
	GetCurrentConsoleFontEx(hStdOut, TRUE, &cfiEx);
	size = GetConsoleFontSize(hStdOut, cfiEx.nFont);
	if (h >= 5 && h <= 72) {
		cfiEx.dwFontSize.Y = (short)h;
		cfiEx.dwFontSize.X = (short)(h / 2);
		SetCurrentConsoleFontEx(hStdOut, TRUE, &cfiEx);
	}
	return (int)size.Y;
}
#endif 


void getpalette(unsigned* palette)
#ifdef CONIOEX
;
#else 
{
	CONSOLE_SCREEN_BUFFER_INFOEX csbiEx;
	int i;
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	ZeroMemory(&csbiEx, sizeof(csbiEx));
	csbiEx.cbSize = sizeof(csbiEx);
	GetConsoleScreenBufferInfoEx(hStdOut, &csbiEx);
	if (!palette) return;
	for (i = 0; i < 16; ++i)
		palette[i] = (unsigned)csbiEx.ColorTable[i];
}
#endif 


#define PALETTE95 ((const unsigned*)MAKEINTRESOURCE(0x9500))
void setpalette(const unsigned* palette)
#ifdef CONIOEX
;
#else 
{
	static const unsigned rgb[16] = {
		0x00000000, 0x00000080, 0x00008000, 0x00008080, 0x00800000, 0x00800080, 0x00808000, 0x00C0C0C0,
		0x00808080, 0x000000FF, 0x0000FF00, 0x0000FFFF, 0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
	};
	static const unsigned rgb95[16] = {
		0x00000000, 0x000000A8, 0x0000A800, 0x0000A8A8, 0x00A80000, 0x00A800A8, 0x00A85400, 0x00A8A8A8,
		0x00545454, 0x005454FC, 0x0054FC54, 0x0054FCFC, 0x00FC54FC, 0x00FC54FC, 0x00FCFC54, 0x00FCFCFC,
	};
	CONSOLE_SCREEN_BUFFER_INFOEX csbiEx;
	int i;
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	ZeroMemory(&csbiEx, sizeof(csbiEx));
	csbiEx.cbSize = sizeof(csbiEx);
	GetConsoleScreenBufferInfoEx(hStdOut, &csbiEx);
	switch ((INT_PTR)palette) {
	case (INT_PTR)0:
		palette = rgb;
		break;
	case (INT_PTR)PALETTE95:
		palette = rgb95;
		break;
	default:
		break;
	}
	for (i = 0; i < 16; ++i)
		csbiEx.ColorTable[i] = (COLORREF)palette[i];
	SetConsoleScreenBufferInfoEx(hStdOut, &csbiEx);
}
#endif 

#endif 

__inline void setcaption(const char *title) {SetConsoleTitleA(title);}
__inline int getcaption(char *title, int len) {return GetConsoleTitleA(title, len);}
__inline void msleep(unsigned long msecs) {Sleep(msecs);}

#define PK_ENTER				VK_RETURN
#define PK_ESC					VK_ESCAPE
#define PK_F1					VK_F1
#define PK_F2					VK_F2
#define PK_F3					VK_F3
#define PK_F4					VK_F4
#define PK_F5					VK_F5
#define PK_F6					VK_F6
#define PK_F7					VK_F7
#define PK_F8					VK_F8
#define PK_F9					VK_F9
#define PK_F10					VK_F10
#define PK_F11					VK_F11
#define PK_F12					VK_F12
#define PK_SP					VK_SPACE
#define PK_UP					VK_UP
#define PK_DOWN					VK_DOWN
#define PK_RIGHT				VK_RIGHT
#define PK_LEFT					VK_LEFT
#define PK_SHIFT				VK_SHIFT
#define PK_CTRL					VK_CONTROL
#define PK_ALT					VK_MENU
#define PK_BS					VK_BACK
#define PK_PAUSE				VK_PAUSE
#define PK_INS					VK_INSERT
#define PK_DEL					VK_DELETE
#define PK_TAB					VK_TAB
#define PK_NFER					VK_KANA		
#define PK_XFER					VK_CONVERT	
#define PK_0					0x30
#define PK_1					0x31
#define PK_2					0x32
#define PK_3					0x33
#define PK_4					0x34
#define PK_5					0x35
#define PK_6					0x36
#define PK_7					0x37
#define PK_8					0x38
#define PK_9					0x39
#define PK_NUMPAD0				VK_NUMPAD0	
#define PK_NUMPAD1				VK_NUMPAD1	
#define PK_NUMPAD2				VK_NUMPAD2	
#define PK_NUMPAD3				VK_NUMPAD3	
#define PK_NUMPAD4				VK_NUMPAD4	
#define PK_NUMPAD5				VK_NUMPAD5	
#define PK_NUMPAD6				VK_NUMPAD6	
#define PK_NUMPAD7				VK_NUMPAD7	
#define PK_NUMPAD8				VK_NUMPAD8	
#define PK_NUMPAD9				VK_NUMPAD9	
#define PK_A					0x41
#define PK_B					0x42
#define PK_C					0x43
#define PK_D					0x44
#define PK_E					0x45
#define PK_F					0x46
#define PK_G					0x47
#define PK_H					0x48
#define PK_I					0x49
#define PK_J					0x4A
#define PK_K					0x4B
#define PK_L					0x4C
#define PK_M					0x4D
#define PK_N					0x4E
#define PK_O					0x4F
#define PK_P					0x50
#define PK_Q					0x51
#define PK_R					0x52
#define PK_S					0x53
#define PK_T					0x54
#define PK_U					0x55
#define PK_V					0x56
#define PK_W					0x57
#define PK_X					0x58
#define PK_Y					0x59
#define PK_Z					0x5A

#define PK_LT					0x0BC		
#define PK_GT					0x0BE		
#define PK_SLUSH				0x0BF		
#define PK_DOT					VK_DECIMAL	
#define PK_DIV					VK_DIVIDE	
#define PK_BSLUSH				0x0E2		

#define PK_SEMICOLON			0x0BB		
#define PK_ADD					VK_ADD		
#define PK_COLON				0x0BA		
#define PK_MUL					VK_MULTIPLY	
#define PK_RBRACE				0x0DD		

#define PK_ATMARK				0x0C0		
#define PK_LBRACE				0x0DB		

#define PK_MINUS				0x0BD		
#define PK_SUB					VK_SUBTRACT	
#define PK_XOR					0x0DE		
#define PK_YEN					0x0DC		

#define PK_KANJI				0x0F3		
#define PK_CAPS					0x0F0		

#define PM_LEFT					VK_LBUTTON
#define PM_MID					VK_MBUTTON
#define PM_RIGHT				VK_RBUTTON
#define PM_CURX					0x0101
#define PM_CURY					0x0102

#define PJ1_XPOS				0x0200
#define PJ1_YPOS				0x0201
#define PJ1_ZPOS				0x0202
#define PJ1_BTNS				0x0203
#define PJ1_RPOS				0x0204
#define PJ1_UPOS				0x0205
#define PJ1_VPOS				0x0206
#define PJ1_POV					0x0207
#define PJ1_BTNO				0x0208
#define PJ2_XPOS				0x0210
#define PJ2_YPOS				0x0211
#define PJ2_ZPOS				0x0212
#define PJ2_BTNS				0x0213
#define PJ2_RPOS				0x0214
#define PJ2_UPOS				0x0215
#define PJ2_VPOS				0x0216
#define PJ2_POV					0x0217
#define PJ2_BTNO				0x0218
#define PJ3_XPOS				0x0220
#define PJ3_YPOS				0x0221
#define PJ3_ZPOS				0x0222
#define PJ3_BTNS				0x0223
#define PJ3_RPOS				0x0224
#define PJ3_UPOS				0x0225
#define PJ3_VPOS				0x0226
#define PJ3_POV					0x0227
#define PJ3_BTNO				0x0228
#define PJ4_XPOS				0x0230
#define PJ4_YPOS				0x0231
#define PJ4_ZPOS				0x0232
#define PJ4_BTNS				0x0233
#define PJ4_RPOS				0x0234
#define PJ4_UPOS				0x0235
#define PJ4_VPOS				0x0236
#define PJ4_POV					0x0237
#define PJ4_BTNO				0x0238
#define PJ_XPOS					PJ1_XPOS
#define PJ_YPOS					PJ1_YPOS
#define PJ_ZPOS					PJ1_ZPOS
#define PJ_BTNS					PJ1_BTNS
#define PJ_RPOS					PJ1_RPOS
#define PJ_UPOS					PJ1_UPOS
#define PJ_VPOS					PJ1_VPOS
#define PJ_POV					PJ1_POV
#define PJ_BTNO					PJ1_BTNO

CONIOEX_INST DWORD	__conioex_h_dwKeyMap[8]
#ifndef CONIOEX
= {0, 0, 0, 0, 0, 0, 0, 0}
#endif 
;
CONIOEX_INST COORD	__conioex_h_crdMouse
#ifndef CONIOEX
= {0, 0}
#endif 
;


void reinport(void)
#ifdef CONIOEX
;
#else 
{
	int i;
	for(i = 0; i < 8; i++)
	{
		__conioex_h_dwKeyMap[i] = 0;
	}
}
#endif 



int inport(int port)
#ifdef CONIOEX
;
#else 
{
	DWORD	dwEvent;
	HANDLE	h;

	
	if ((port & 0xfe00) == 0x0200) {
		int			id = (port & 0x01f0) >> 4;
		int			func = port & 0x0f;
		JOYINFOEX	ji;
		ZeroMemory(&ji, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNALL;
		switch (func) {
		case 0:	case 1:	case 2:	case 3:
		case 4:	case 5:	case 6:	case 7:	case 8:
			if (joyGetPosEx(id, &ji) != JOYERR_NOERROR)
				return -1;
			if (ji.dwXpos == 0 &&
				ji.dwYpos == 0 &&
				ji.dwZpos == 0 &&
				ji.dwRpos == 0 &&
				ji.dwUpos == 0 &&
				ji.dwVpos == 0 &&
				ji.dwButtons == 0 &&
				ji.dwButtonNumber == 0 &&
				ji.dwPOV == 0) return -1;
			switch (func) {
			case 0:
				return ji.dwXpos;
			case 1:
				return ji.dwYpos;
			case 2:
				return ji.dwZpos;
			case 3:
				return ji.dwButtons;
			}
			break;
		default:
			break;
		}
		return 0;
	}
	
	dwEvent = 0;
	h = GetStdHandle(STD_INPUT_HANDLE);

	
	GetConsoleMode(h, &dwEvent);
	SetConsoleMode(h, dwEvent | ENABLE_MOUSE_INPUT);
	dwEvent = 0;
	

	if (GetNumberOfConsoleInputEvents(h, &dwEvent) && dwEvent) {
		DWORD			dwRead = 0;
		PINPUT_RECORD	pInpRec = (PINPUT_RECORD)_alloca(dwEvent * sizeof(INPUT_RECORD));

		if (ReadConsoleInput(h, pInpRec, dwEvent, &dwRead) && dwRead) {
			PINPUT_RECORD	pir = pInpRec;
			DWORD			dw;
			for (dw = 0; dw < dwRead; dw++, pir++) {
				switch (pir->EventType) {
				case KEY_EVENT: {
					KEY_EVENT_RECORD*	pker = &pir->Event.KeyEvent;
					if (pker->wVirtualKeyCode > 0x0FF)
						break;
					if (pker->bKeyDown)
						__conioex_h_dwKeyMap[pker->wVirtualKeyCode >> 5] |= (0x01 << (pker->wVirtualKeyCode & 31));
					else
						__conioex_h_dwKeyMap[pker->wVirtualKeyCode >> 5] &= ~(0x01 << (pker->wVirtualKeyCode & 31));
					if (pker->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
						__conioex_h_dwKeyMap[VK_MENU >> 5] |= (0x01 << (VK_MENU & 31));
					else
						__conioex_h_dwKeyMap[VK_MENU >> 5] &= ~(0x01 << (VK_MENU & 31));
					if (pker->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
						__conioex_h_dwKeyMap[VK_CONTROL >> 5] |= (0x01 << (VK_CONTROL & 31));
					else
						__conioex_h_dwKeyMap[VK_CONTROL >> 5] &= ~(0x01 << (VK_CONTROL & 31));
					if (pker->dwControlKeyState & SHIFT_PRESSED)
						__conioex_h_dwKeyMap[VK_SHIFT >> 5] |= (0x01 << (VK_SHIFT & 31));
					else
						__conioex_h_dwKeyMap[VK_SHIFT >> 5] &= ~(0x01 << (VK_SHIFT & 31));
					break;
				}
				case MOUSE_EVENT: {
					MOUSE_EVENT_RECORD	*pmer = &pir->Event.MouseEvent;
					__conioex_h_crdMouse = pmer->dwMousePosition;
					if (pmer->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
						__conioex_h_dwKeyMap[VK_LBUTTON >> 5] |= (0x01 << (VK_LBUTTON & 31));
					else
						__conioex_h_dwKeyMap[VK_LBUTTON >> 5] &= ~(0x01 << (VK_LBUTTON & 31));
					if (pmer->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED)
						__conioex_h_dwKeyMap[VK_MBUTTON >> 5] |= (0x01 << (VK_MBUTTON & 31));
					else
						__conioex_h_dwKeyMap[VK_MBUTTON >> 5] &= ~(0x01 << (VK_MBUTTON & 31));
					if (pmer->dwButtonState & RIGHTMOST_BUTTON_PRESSED)
						__conioex_h_dwKeyMap[VK_RBUTTON >> 5] |= (0x01 << (VK_RBUTTON & 31));
					else
						__conioex_h_dwKeyMap[VK_RBUTTON >> 5] &= ~(0x01 << (VK_RBUTTON & 31));
					break;
				}
				default:
					break;
				}
			}
		}
	}

	
	switch (port) {
	case PM_CURX:
		return __conioex_h_crdMouse.X + 1;
	case PM_CURY:
		return __conioex_h_crdMouse.Y + 1;
	default:
		break;
	}
	
	return (__conioex_h_dwKeyMap[(port & 0x0FF) >> 5] & (0x01 << (port & 31))) != 0;
}
#endif 

typedef struct {
	int				nDevType;
	MCIDEVICEID		wDeviceID;
	char			szPath[MAX_PATH];
	int				nRepeat;
} __conioex_h_SoundInfo;


INT_PTR opensound(char *path)
#ifdef CONIOEX
;
#else 
{
	const char	szMidiExt[] = ".mid|.midi|.rmi";
	const char	szWaveExt[] = ".wav|.wave";
	const char	szMP3Ext[] = ".mp3";
	char		szExt[_MAX_EXT];
	union {
		MCI_WAVE_OPEN_PARMSA	wop;
		MCI_OPEN_PARMSA			op;
	} m;
	DWORD_PTR	dwCmd;
	__conioex_h_SoundInfo	*psi;

	psi = (__conioex_h_SoundInfo *)malloc(sizeof(__conioex_h_SoundInfo));
	if (psi == NULL)
		return 0;
	ZeroMemory(psi, sizeof(*psi));

	ZeroMemory(&m, sizeof(m));
	_splitpath(path, NULL, NULL, NULL, szExt);
	strlwr(szExt);
	dwCmd = MCI_OPEN_TYPE | MCI_OPEN_ELEMENT;
	if (strstr(szMidiExt, szExt)) {
		psi->nDevType = MCI_DEVTYPE_SEQUENCER;
		lstrcpynA(psi->szPath, path, MAX_PATH);
		m.op.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_SEQUENCER;
		m.op.lpstrElementName = psi->szPath;
		dwCmd |= MCI_OPEN_TYPE_ID;
	} else if (strstr(szWaveExt, szExt)) {
		psi->nDevType = MCI_DEVTYPE_WAVEFORM_AUDIO;
		lstrcpynA(psi->szPath, path, MAX_PATH);
		m.wop.lpstrDeviceType  = (LPCSTR)MCI_DEVTYPE_WAVEFORM_AUDIO;
		m.wop.lpstrElementName = psi->szPath;
		dwCmd |= MCI_OPEN_TYPE_ID;


	} else if (strstr(szMP3Ext, szExt)) {
		psi->nDevType = MCI_DEVTYPE_DIGITAL_VIDEO;
		lstrcpynA(psi->szPath, path, MAX_PATH);
		m.op.lpstrDeviceType = "MPEGVideo";
		m.op.lpstrElementName = psi->szPath;
	} else {
		free(psi);
		return 0;
	}
	if (mciSendCommandA(0, MCI_OPEN, dwCmd, (DWORD_PTR)&m)) {
		free(psi);
		return 0;
	}
	psi->wDeviceID = m.op.wDeviceID;
	return (INT_PTR)psi;
}
#endif 


void closesound(INT_PTR hsound)
#ifdef CONIOEX
;
#else 
{
	__conioex_h_SoundInfo	*psi;

	if (!hsound)
		return;
	psi = (__conioex_h_SoundInfo *)hsound;
	if (psi->wDeviceID) {
		mciSendCommand(psi->wDeviceID, MCI_CLOSE, 0, 0);
		psi->wDeviceID = 0;
	}
	free(psi);
}
#endif 


void playsound(INT_PTR hsound, int repeat)
#ifdef CONIOEX
;
#else 
{
	__conioex_h_SoundInfo	*psi;
	DWORD_PTR				dwCmd;
	MCI_PLAY_PARMS			mpp;

	if (!hsound)
		return;
	psi = (__conioex_h_SoundInfo *)hsound;
	if (!psi->wDeviceID)
		return;
	psi->nRepeat = repeat;
	ZeroMemory(&mpp, sizeof(mpp));
	dwCmd = 0;
	if (repeat) {
		switch (psi->nDevType) {
		case MCI_DEVTYPE_DIGITAL_VIDEO:
			dwCmd |= (MCI_FROM | MCI_DGV_PLAY_REPEAT);
			mpp.dwFrom = 0;
			break;
		case MCI_DEVTYPE_SEQUENCER:
		case MCI_DEVTYPE_WAVEFORM_AUDIO:
			break;
		default:
			break;
		}
	}
	mciSendCommand(psi->wDeviceID, MCI_SEEK, MCI_SEEK_TO_START, 0);
	mciSendCommand(psi->wDeviceID, MCI_PLAY, dwCmd, (DWORD_PTR)&mpp);
}
#endif 


void stopsound(INT_PTR hsound)
#ifdef CONIOEX
;
#else 
{
	__conioex_h_SoundInfo	*psi;

	if (!hsound)
		return;
	psi = (__conioex_h_SoundInfo *)hsound;
	if (!psi->wDeviceID)
		return;
	psi->nRepeat = 0;
	mciSendCommand(psi->wDeviceID, MCI_STOP, MCI_WAIT, 0);
	mciSendCommand(psi->wDeviceID, MCI_SEEK, MCI_SEEK_TO_START, 0);
}
#endif 


int checksound(INT_PTR hsound)
#ifdef CONIOEX
;
#else 
{
	__conioex_h_SoundInfo	*psi;
	MCI_STATUS_PARMS		msp;

	if (!hsound)
		return 0;
	psi = (__conioex_h_SoundInfo *)hsound;
	if (!psi->wDeviceID)
		return 0;
	ZeroMemory(&msp, sizeof(msp));
	msp.dwItem = MCI_STATUS_MODE;
	if (mciSendCommand(psi->wDeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&msp))
		return 0;
	return msp.dwReturn == MCI_MODE_PLAY;
}
#endif 


void updatesound(INT_PTR hsound)
#ifdef CONIOEX
;
#else 
{
	__conioex_h_SoundInfo	*psi;
	MCI_STATUS_PARMS		msp;

	if (!hsound)
		return;
	psi = (__conioex_h_SoundInfo *)hsound;
	if (!psi->wDeviceID || !psi->nRepeat)
		return;
	switch (psi->nDevType) {
	case MCI_DEVTYPE_DIGITAL_VIDEO:
		break;
	case MCI_DEVTYPE_SEQUENCER:
	case MCI_DEVTYPE_WAVEFORM_AUDIO:
		ZeroMemory(&msp, sizeof(msp));
		msp.dwItem = MCI_STATUS_MODE;
		if (!mciSendCommand(psi->wDeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&msp)) {
			if (msp.dwReturn != MCI_MODE_PLAY) {
				if (psi->nRepeat > 0) {
					psi->nRepeat = -1;
					mciSendCommand(psi->wDeviceID, MCI_SEEK, MCI_SEEK_TO_START, 0);
					mciSendCommand(psi->wDeviceID, MCI_PLAY, 0, 0);
				}
			} else
				psi->nRepeat = 1;
		}
		break;
	default:
		break;
	}
}
#endif 


void setvolume(INT_PTR hsound, int percent)
#ifdef CONIOEX
;
#else 
{
	__conioex_h_SoundInfo	*psi;

	if (!hsound)
		return;
	psi = (__conioex_h_SoundInfo *)hsound;
	if (!psi->wDeviceID)
		return;
	switch (psi->nDevType) {
	case MCI_DEVTYPE_DIGITAL_VIDEO: {
		MCI_DGV_SETAUDIO_PARMS	mdsp;

		ZeroMemory(&mdsp, sizeof(mdsp));
		mdsp.dwItem = MCI_DGV_SETAUDIO_VOLUME;
		mdsp.dwValue = percent * 10;
		mciSendCommand(psi->wDeviceID, MCI_SETAUDIO,
			MCI_DGV_SETAUDIO_ITEM | MCI_DGV_SETAUDIO_VALUE, (DWORD_PTR)&mdsp);
		break;
	}
	case MCI_DEVTYPE_SEQUENCER: {
		DWORD dwVolume;

		dwVolume = 0x0ffff * percent / 100;
		midiOutSetVolume(0, (DWORD)MAKELONG(dwVolume, dwVolume));
		break;
	}
	case MCI_DEVTYPE_WAVEFORM_AUDIO: {
		DWORD dwVolume;

		dwVolume = 0x0ffff * percent / 100;
		waveOutSetVolume(0, (DWORD)MAKELONG(dwVolume, dwVolume));
		break;
	}
	default:
		break;
	}
}
#endif 

#ifdef __cplusplus
}
#endif

#endif 
