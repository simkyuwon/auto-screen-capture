#include <iostream>
#include <ctime>
#include <cstring>
#include <vector>
#include <windows.h>
#include <direct.h>
#include <gdiplus.h>
#pragma comment(lib,"gdiplus")

#define BYTE_CHK(n) (n<0?0:(n>=256?255:n))

using namespace std;
#pragma pack(push, 1)
typedef struct PIXEL_ARGB{
	BYTE B;
	BYTE G;
	BYTE R;
	BYTE A;
}pixelARGB;
#pragma pack(pop)
int Height = GetSystemMetrics(SM_CYSCREEN);
int Width = GetSystemMetrics(SM_CXSCREEN);
	
bool dirExists(const string& dirName_in);
void ConvertCtoWC(const char *str, wchar_t *wstr);
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
void gdiscreen(const WCHAR* filename, bool save);
void BitmapGrayscale(Gdiplus::Bitmap* bitmap);

int main(int argc, char* argv[]){
	AllocConsole();//init concole
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	wchar_t filenameWC[256];
	char filename[128];
	char foldername[128];
	char path[128];
	char buf[256];
	time_t curr_time;
	time_t prev_time = 0;
	struct tm *curr_tm;
	int saveDelay = 5;

	sprintf(buf,"%s\\..\\screenCapture", argv[0]);
	_fullpath(path, buf, sizeof(path));

	if(!dirExists(path))
		mkdir(path);
	
	while(true){
		time(&curr_time);
		curr_tm = localtime(&curr_time);
		strftime(foldername, sizeof(foldername), "%y%m%d", curr_tm);
		if(!dirExists(foldername)){
			sprintf(buf, "%s\\%s", path, foldername);
			mkdir(buf);
		}
		strftime(filename, sizeof(filename), "%H-%M-%S", curr_tm);
		sprintf(buf, "%s\\%s\\%s.jpeg", path, foldername, filename);
		ConvertCtoWC(buf, filenameWC);
		if(difftime(curr_time, prev_time) >= saveDelay){
			time(&prev_time);
			gdiscreen(filenameWC, true);
		}
		else
			gdiscreen(filenameWC, false);
	}
	return 0;
}

void ConvertCtoWC(const char *str, wchar_t *wstr){
	MultiByteToWideChar(CP_ACP, 0, str, strlen(str)+1, wstr, MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, NULL));
}

bool dirExists(const string& dirName_in){
	DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES)
		return false;	//something is wrong with your path!

	if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
		return true;	 // this is a directory!

	return false;		// this is not a directory!
}

void gdiscreen(const WCHAR* filename, bool save){
	using namespace Gdiplus;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(0);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, 0, 0, SRCCOPY);

		Bitmap bitmap(membit, NULL);
		BitmapGrayscale(&bitmap);
	
		CLSID clsid;
		GetEncoderClsid(L"image/jpeg", &clsid);
		if(save)
			bitmap.Save(filename, &clsid, NULL);
		DeleteObject(memdc);
		DeleteObject(membit);
		::ReleaseDC(0,scrdc);
	}
	GdiplusShutdown(gdiplusToken);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid){
	using namespace Gdiplus;
	UINT	num = 0;	// number of image encoders
	UINT	size = 0;	// size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if(size == 0)
		return -1;	// Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo == NULL)
		return -1;	// Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;	// Success
		}		
	}

	free(pImageCodecInfo);
	return 0;
}

void BitmapGrayscale(Gdiplus::Bitmap* bitmap){
	using namespace Gdiplus;
	auto *bitmapData = new BitmapData;
	Rect rect(0, 0, Width, Height);
	bitmap->LockBits(&rect, 3, PixelFormat32bppARGB, bitmapData);

	pixelARGB *pixels = (pixelARGB *)(bitmapData->Scan0);
	pixelARGB pixel;

	for(int j = 0; j < Height; j++){
		for(int i = 0; i < Width; i++){
			int idx = (j * Width) + i;
			pixel = pixels[idx];
			BYTE Y = BYTE_CHK(0.2126 * pixel.R + 0.7152 * pixel.G + 0.0722 * pixel.B);
			pixels[idx] = {Y, Y, Y, 0};
		}
	}

	bitmap->UnlockBits(bitmapData);
	free(bitmapData);
}
