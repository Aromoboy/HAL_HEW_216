#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <assert.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


#include <stdarg.h>
#include <stdio.h>

static const int FPS = 60;
static const float fFPS = static_cast<float>(FPS);

// リソースパス
#define ASSET(path)	"Assets/"path

// 3D空間定義
#define CMETER(value) (value * 0.01f)
#define METER(value) (value * 1.0f)
#define MSEC(value) (value / fFPS)
#define CMSEC(value) MSEC(CMETER(value))
static const float GRAVITY = 0.98f;

// タイトル
static const wchar_t* APP_TITLE = L"HEW_Team216";

// 画面サイズ
static const int SCREEN_WIDTH	= 1280;
static const int SCREEN_HEIGHT	= 720;

// グリッドサイズ(デバッグ用
static const int	DEBUG_GRID_NUM		= 10;			// グリッド中心から端までの線の本数
static const float	DEBUG_GRID_MARGIN	= METER(1.0f);	// グリッド配置幅

static const float PI = 3.14159265358979323846f;
static const float DEG_TO_RAD = PI / 180.0f;
static const float RAD_TO_DEG = 180.0f / PI;

#endif
