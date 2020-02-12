#include <iostream>
#include <ctime>
#include <cstring>
#include <vector>
#include <windows.h>
#include <direct.h>
#include <gdiplus.h>
#pragma comment(lib,"gdiplus")

using namespace std;

bool dirExists(const string& dirName_in);
void ConvertCtoWC(const char *str, wchar_t *wstr);
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
void gdiscreen(const WCHAR* filename);

int main(){
	AllocConsole();//init concole
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	wchar_t filenameWC[256];
	char filename[128];
	char foldername[128];
	char path[128] = "C:\\Users\\Administrator\\Downloads";
	char buf[256];
	time_t curr_time;
	struct tm *curr_tm;
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
		gdiscreen(filenameWC);
		while(!difftime(curr_time ,time(NULL)));
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

void gdiscreen(const WCHAR* filename){
	using namespace Gdiplus;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(0);
		int Height = GetSystemMetrics(SM_CYSCREEN);
		int Width = GetSystemMetrics(SM_CXSCREEN);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		HBITMAP hOldBitmap =(HBITMAP) SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, 0, 0, SRCCOPY);

		Bitmap bitmap(membit, NULL);
		CLSID clsid;
		GetEncoderClsid(L"image/jpeg", &clsid);
		bitmap.Save(filename, &clsid, NULL);

		SelectObject(memdc, hOldBitmap);
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
