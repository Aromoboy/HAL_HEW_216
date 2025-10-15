#ifndef ___DATA_H___
#define ___DATA_H___


#define PI (3.1415)



#define WINX (960)
#define WINY (520)


struct float3 {
	float x;
	float y;
	float z;
};


struct float2 {
	float x;
	float y;

	float2() : x(0), y(0) {}
	float2(float _x, float _y) : x(_x), y(_y) {}

	float2 operator+(const float2& rhs) const {
		return float2(x + rhs.x, y + rhs.y);
	}

	float2 operator-(const float2& rhs) const {
		return float2(x - rhs.x, y - rhs.y);
	}

	float2 operator*(float s) const {
		return float2(x * s, y * s);
	}

	float2 operator/(float s) const {
		return float2(x / s, y / s);
	}


	float2& operator+=(const float2& rhs) { x += rhs.x; y += rhs.y; return *this; }

};


struct float4 {
	float w;
	float x;
	float y;
	float z;
};

#endif

