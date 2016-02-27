#include <Windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <math.h>
#include <array>

int screen_width = 640;
int screen_height = 640;
float angle = 0.0;
HDC hdc_backbuffer;
HBITMAP backbuffer;

struct Vec2
{
	float x;
	float y;

	Vec2(float _x, float _y)
	{
		x = _x;
		y = _y;
	};

	Vec2 operator+(const Vec2& rhs)
	{
		return Vec2(this->x + rhs.x, this->y + rhs.y);
	}

	Vec2 operator-(const Vec2& rhs)
	{
		return Vec2(this->x - rhs.x, this->y - rhs.y);
	}

	float length()
	{
		return sqrt(x*x + y*y);
	}

	Vec2 xy()
	{
		return Vec2(x, y);
	}
};

struct Vec3
{
	float x;
	float y;
	float z;

	Vec3(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	};

	Vec3(const Vec2& _xy, float _z)
	{
		x = _xy.x;
		y = _xy.y;
		z = _z;
	};

	Vec3 operator+(const Vec3& rhs)
	{
		return Vec3(this->x + rhs.x, this->y + rhs.y, this->z + rhs.z);
	}

	Vec3 operator-(const Vec3& rhs)
	{
		return Vec3(this->x - rhs.x, this->y - rhs.y, this->z - rhs.z);
	}

	Vec3 operator*(float rhs)
	{
		return Vec3(x*rhs, y*rhs, z*rhs);
	}

	Vec2 xy()
	{
		return Vec2(x, y);
	}
};

struct Vertex
{
	Vec3 pos;
	Vec3 color;

	Vertex(const Vec3& _pos, const Vec3& _color)
		: pos(_pos), color(_color)
	{}
};

const int vertex_count = 36;


template<class T, class T1>
T interpolate(T1 y0, T1 y1, T x0, T x1, T x)
{
	return y0 + (y1 - y0)*(x - x0) / (x1 - x0);
}

/*struct Rgb
{
	int red;
	int green;
	int blue;

	Rgb interpolateColor(Vec2 v1, Vec2 v2, Rgb v1_c, Rgb v2_c, Vec2 v)
	{
		Rgb interpolated;
		interpolated.red = static_cast<int>(interpolate(v1_c.red, v2_c.red, v1.x, v2.x, v.x));
		interpolated.green = static_cast<int>((interpolate(v1_c.green, v2_c.green, v1.x, v2.x, v.x) + interpolate(v1_c.green, v2_c.green, v1.x, v2.x, v.x)) / 2);
		interpolated.blue = static_cast<int>((interpolate(v1_c.blue, v2_c.blue, v1.x, v2.x, v.x) + interpolate(v1_c.blue, v2_c.blue, v1.x, v2.x, v.x)) / 2);
	};
}; */




Vec2 CameraToScreenSpace(Vec2 val)
{
	Vec2 converted(0, 0);
	converted.x = screen_width * (1 + val.x) / 2;
	converted.y = screen_height * (1 - val.y) / 2;

	return converted;
}

Vec3 RotateAroundY(Vec3 vertex, float angle)
{
	Vec3 result(0, 0, 0);
	result.x = vertex.x*cos(angle) + vertex.z*sin(angle);
	result.y = vertex.y; // rotating around Y we dont act with Y at all
	result.z = -vertex.x*sin(angle) + vertex.z*cos(angle);
	return result;
}

Vec3 RotateAroundX(Vec3 vertex, float angle)
{
	Vec3 result(0, 0, 0);
	result.x = vertex.x + vertex.z*sin(angle);
	result.y = vertex.y*cos(angle) + vertex.z*sin(angle);
	result.z = -vertex.y*sin(angle) + vertex.z*cos(angle);
	return result;
}

void DrawLine(HDC hdc, int x0, int y0, int x1, int y1, Vec3 col0, Vec3 col1)
{
	float dx = x1 - x0;
	float dy = y1 - y0;
	if (abs(dx) >= abs(dy))
	{
		float stepy = dy / abs(dx);
		float stepx = dx / abs(dx);
		float currY = y0;
		float currX = x0;
		for (; currX != x1; currX += stepx)
		{
			Vec2 cur = Vec2(currX, round(currY)) - Vec2(x0, y0);
			float alpha = cur.length() / Vec2(dx, dy).length();
			Vec3 color = col0 + (col1 - col0)*alpha;
			SetPixel(hdc, currX, round(currY), RGB(color.x, color.y, color.z));
			currY += stepy;
		}
	}
	else
	{
		float stepx = dx / abs(dy);
		float stepy = dy / abs(dy);
		float currX = x0;
		float currY = y0;
		for (; currY != y1; currY += stepy)
		{
			Vec2 cur = Vec2(round(currX), currY) - Vec2(x0, y0);
			float alpha = cur.length() / Vec2(dx, dy).length();
			Vec3 color = col0 + (col1 - col0)*alpha;
			SetPixel(hdc, round(currX), currY, RGB(color.x, color.y, color.z));
			currX += stepx;
		}
	}
}

float pseudo_cross(const Vec2& v1, const Vec2& v2)
{
	return v1.x * v2.y - v2.x * v1.y;
}

void RasterizeTriangle(HDC hDC, Vec3 v1, Vec3 v2, Vec3 v3, Vec3 col1, Vec3 col2, Vec3 col3)
{
	int minX = min(v1.x, min(v2.x, v3.x));
	int minY = min(v1.y, min(v2.y, v3.y));
	int maxX = max(v1.x, max(v2.x, v3.x));
	int maxY = max(v1.y, max(v2.y, v3.y));

	// Clip against screen bounds
	minX = max(minX, 0);
	minY = max(minY, 0);
	maxX = min(maxX, screen_width - 1);
	maxY = min(maxY, screen_height - 1);

	Vec2 p(0, 0);
	for (p.y = minY; p.y <= maxY; p.y++) {
		for (p.x = minX; p.x <= maxX; p.x++) {
			// Determine barycentric coordinates
			float w1 = pseudo_cross(v3.xy() - v2.xy(), p - v2.xy());
			float w2 = pseudo_cross(v1.xy() - v3.xy(), p - v3.xy());
			float w3 = pseudo_cross(v2.xy() - v1.xy(), p - v1.xy());

			// If p is on or inside all edges, render pixel.
			if (w1 >= 0 && w2 >= 0 && w3 >= 0)
			{
				float full_area = (w1 + w2 + w3) / 2;
				w1 /= full_area*2;
				w2 /= full_area*2;
				w3 /= full_area*2;
				Vec3 color = col1*w1 + col2*w2 + col3*w3;
				SetPixel(hDC, p.x, p.y, RGB(color.x, color.y , color.z));
			}
		}
	}
}

void DrawTriangle(HDC hDC, Vertex p1, Vertex p2, Vertex p3)
{
	// just forget about Z coord, projection to XY is Z-independent
	Vec2 point1 = CameraToScreenSpace(Vec2(p1.pos.x, p1.pos.y));
	Vec2 point2 = CameraToScreenSpace(Vec2(p2.pos.x, p2.pos.y));
	Vec2 point3 = CameraToScreenSpace(Vec2(p3.pos.x, p3.pos.y));
	Vec3 pos1 = Vec3(point1, p1.pos.z);
	Vec3 pos2 = Vec3(point2, p2.pos.z);
	Vec3 pos3 = Vec3(point3, p3.pos.z);
	RasterizeTriangle(hDC, pos1, pos2, pos3, p1.color, p2.color, p3.color);
	//DrawLine(hDC, point1.x, point1.y, point2.x, point2.y, p1.color, p2.color);
	//DrawLine(hDC, point2.x, point2.y, point3.x, point3.y, p2.color, p3.color);
	//DrawLine(hDC, point3.x, point3.y, point1.x, point1.y, p3.color, p1.color);
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_PAINT:
	{
		HDC hDC = GetDC(hWnd);
		SelectObject(hdc_backbuffer, backbuffer);

		RECT rect = { 0, 0, screen_width, screen_height };
		FillRect(hdc_backbuffer, &rect, 0);
		// initialize array of vertices
		// initialize array of vertices
		Vertex vertices[vertex_count] =
		{
			Vertex(Vec3(-0.5f, -0.5f, -0.5f),Vec3(255,0,0)),
			Vertex(Vec3(-0.5f, -0.5f, 0.5f),Vec3(255,0,0)),
			Vertex(Vec3(-0.5f, 0.5f, 0.5f),Vec3(255,0,0)),

			Vertex(Vec3(0.5f, 0.5f, -0.5f),Vec3(0,0,255)),
			Vertex(Vec3(-0.5f, -0.5f, -0.5f),Vec3(0,0,255)),
			Vertex(Vec3(-0.5f, 0.5f, -0.5f),Vec3(0,0,255)),

			Vertex(Vec3(0.5f, -0.5f, 0.5f),Vec3(0,255,0)),
			Vertex(Vec3(-0.5f, -0.5f, -0.5f),Vec3(0,255,0)),
			Vertex(Vec3(0.5f, -0.5f, -0.5f),Vec3(0,255,0)),

			Vertex(Vec3(0.5f, 0.5f, -0.5f),Vec3(0,0,255)),
			Vertex(Vec3(0.5f, -0.5f, -0.5f),Vec3(0,0,255)),
			Vertex(Vec3(-0.5f, -0.5f, -0.5f),Vec3(0,0,255)),

			Vertex(Vec3(-0.5f, -0.5f, -0.5f),Vec3(255,0,0)),
			Vertex(Vec3(-0.5f, 0.5f, 0.5f),Vec3(255,0,0)),
			Vertex(Vec3(-0.5f, 0.5f, -0.5f),Vec3(255,0,0)),

			Vertex(Vec3(0.5f, -0.5f, 0.5f),Vec3(0,255,0)),
			Vertex(Vec3(-0.5f, -0.5f, 0.5f),Vec3(0,255,0)),
			Vertex(Vec3(-0.5f, -0.5f, -0.5f),Vec3(0,255,0)),

			Vertex(Vec3(-0.5f, 0.5f, 0.5f),Vec3(255,255,0)),
			Vertex(Vec3(-0.5f, -0.5f, 0.5f),Vec3(255,255,0)),
			Vertex(Vec3(0.5f, -0.5f, 0.5f),Vec3(255,255,0)),

			Vertex(Vec3(0.5f, 0.5f, 0.5f),Vec3(0,255,255)),
			Vertex(Vec3(0.5f, -0.5f, -0.5f),Vec3(0,255,255)),
			Vertex(Vec3(0.5f, 0.5f, -0.5f),Vec3(0,255,255)),

			Vertex(Vec3(0.5f, -0.5f, -0.5f),Vec3(0,255,255)),
			Vertex(Vec3(0.5f, 0.5f, 0.5f),Vec3(0,255,255)),
			Vertex(Vec3(0.5f, -0.5f, 0.5f),Vec3(0,255,255)),

			Vertex(Vec3(0.5f, 0.5f, 0.5f),Vec3(255,0,255)),
			Vertex(Vec3(0.5f, 0.5f, -0.5f),Vec3(255,0,255)),
			Vertex(Vec3(-0.5f, 0.5f, -0.5f),Vec3(255,0,255)),

			Vertex(Vec3(0.5f, 0.5f, 0.5f),Vec3(255,0,255)),
			Vertex(Vec3(-0.5f, 0.5f, -0.5f),Vec3(255,0,255)),
			Vertex(Vec3(-0.5f, 0.5f, 0.5f),Vec3(255,0,255)),

			Vertex(Vec3(0.5f, 0.5f, 0.5f),Vec3(255,255,0)),
			Vertex(Vec3(-0.5f, 0.5f, 0.5f),Vec3(255,255,0)),
			Vertex(Vec3(0.5f, -0.5f, 0.5f),Vec3(255,255,0))
		};
		
		/*Vertex vertices[vertex_count] =
		{
			Vertex(Vec3(-0.5, -0.5, 0), Vec3(255, 0, 0)),
			Vertex(Vec3(0, 0.5, 0), Vec3(0, 255, 0)),
			Vertex(Vec3(0.5, -0.5, 0), Vec3(0, 0, 255))
		};*/

		/*for (int i = 0; i < 36; ++i)
		{
			vertices[i] = RotateAroundX(vertices[i], 0.3);
			vertices[i] = RotateAroundY(vertices[i], angle);
		}
		for (int i = 0; i < 36; i += 3)
			DrawTriangle(hdc_backbuffer, vertices[i], vertices[i + 1], vertices[i + 2]);*/
		for (int i = 0; i < vertex_count; ++i)
		{
			vertices[i].pos = RotateAroundY(vertices[i].pos, angle);
			vertices[i].pos = RotateAroundX(vertices[i].pos, 0.3);
		}
		for (int i = 0; i < vertex_count; i += 3)
			DrawTriangle(hdc_backbuffer, vertices[i], vertices[i + 1], vertices[i + 2]);

		angle += 0.001;

		BitBlt(hDC, 0, 0, screen_width, screen_height, hdc_backbuffer, 0, 0, SRCCOPY);
		ReleaseDC(hWnd, hDC);
	}

	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

int main()
{
	HINSTANCE hinstance = GetModuleHandle(NULL);

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hinstance;
	wcex.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_APPLICATION));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "PH_MainWindow";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL,
			"Call to RegisterClassEx failed!",
			"Win32 Lesson",
			NULL);

		return 1;
	}

	HWND hwnd;
	int x, w, y, h;
	y = 100; h = 640;
	x = 100; w = 640;
	hwnd = CreateWindow("PH_MainWindow", "WinAPI Test",
		WS_VISIBLE | WS_SIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
		x, y, w, h,
		0, 0,
		hinstance, NULL);

	HDC hdc = GetDC(hwnd);
	hdc_backbuffer = CreateCompatibleDC(hdc);
	backbuffer = CreateCompatibleBitmap(hdc, screen_width, screen_height);

	MSG msg;
	while (GetMessage(&msg, hwnd, 0, 0))
	{
		if (msg.message == WM_QUIT)
			return 0;
		DispatchMessage(&msg);
	}

	DeleteObject(backbuffer);
	DeleteDC(hdc_backbuffer);
	ReleaseDC(hwnd, hdc);

	return 0;
}