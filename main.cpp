#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>
#include <utility>
#include <stack>
#include <map>
#include <csignal>
#include <windows.h>
#include <direct.h>
#include <gdiplus.h>
#include <process.h>

#pragma comment(lib,"gdiplus")

#define BYTE_CHK(n) (n<0?0:(n>=256?255:n))
#define absDiff(a,b) (a>b?a-b:b-a)

using namespace std;
#pragma pack(push, 1)
typedef struct PIXEL_ARGB{
	BYTE B;
	BYTE G;
	BYTE R;
	BYTE A;
}pixelARGB;
#pragma pack(pop)

enum Command{
	CMD_HELP = 1,
	CMD_QUIT,
	CMD_SETDELAY,
	CMD_SETTIMER,
	CMD_SETDEBUG,
	CMD_CLEAR,
	CMD_INFO,
};

static map <string, int> cmd;

HANDLE hThread;
DWORD threadID;
int MonitorX = GetSystemMetrics(SM_XVIRTUALSCREEN);
int MonitorY = GetSystemMetrics(SM_YVIRTUALSCREEN);
int Height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
int Width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
int resizeHeight = Height/2;
int resizeWidth = Width/2;
int maximumSaveDelay = 10, minimumSaveDelay = 5;//sec
int timerDelay = 20;//fps
int deleteDelay = 3;//day
char path[128];
time_t prev_time = 0;
bool debug = false;

void CALLBACK routine(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime);
void exitHandler(int signum);
void init();
void initSetting();
bool dirExists(const string& dirName_in);
void ConvertCtoWC(const char *str, wchar_t *wstr);
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
bool gdiscreen(const char* filename, bool maximum, bool minimum);
int bfs();
unsigned int WINAPI keyboardInput(void *args);

BYTE *imgArr, *prevImgArr, *diffImgArr;

int main(int argc, char* argv[]){
	char buf[256];
	int key = 0;
	MSG msg;

	sprintf(buf,"%s\\..\\screenCapture", argv[0]);
	_fullpath(path, buf, sizeof(path));

	init();

	while(GetMessage(&msg, NULL, 0, 0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0)
			break; 
	}

	delete[] imgArr;
	delete[] prevImgArr;
	delete[] diffImgArr;

	KillTimer(NULL, 0);

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

bool gdiscreen(const char* filename, bool maximum, bool minimum){
	using namespace Gdiplus;
	char buf[256];
	wchar_t WCbuf[256];
	bool save = false;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(NULL);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, MonitorX, MonitorY, SRCCOPY);

		Bitmap *origin = new Bitmap(membit, NULL);
		Bitmap *bitmap = new Bitmap(resizeWidth, resizeHeight, PixelFormat32bppARGB);

		//grayscale		
		BitmapData *bitmapData = new BitmapData;
		Rect rect(0, 0, Width, Height);
		origin->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, bitmapData);

		pixelARGB *pixels = (pixelARGB *)(bitmapData->Scan0);
		pixelARGB pixel;

		for(int j = 0; j < Height; j += 2){
			for(int i = 0; i < Width; i += 2){
				pixel = pixels[j * Width + i];
				imgArr[(j * resizeWidth + i) / 2] = BYTE_CHK(0.2126 * pixel.R + 0.7152 * pixel.G + 0.0722 * pixel.B);
			}
		}

		origin->UnlockBits(bitmapData);

		//difference image
		Rect arrRect(0, 0, resizeWidth, resizeHeight);
		bitmap->LockBits(&arrRect, ImageLockModeWrite, PixelFormat32bppARGB, bitmapData);

		pixels = (pixelARGB *)(bitmapData->Scan0);
		for(int j = 0; j < resizeHeight; j++){
			for(int i = 0; i < resizeWidth ; i++){
				int idx = j * resizeWidth + i;
				diffImgArr[idx] = absDiff(imgArr[idx], prevImgArr[idx]);
				pixels[idx] = {diffImgArr[idx], diffImgArr[idx], diffImgArr[idx], 0};
				prevImgArr[idx] = prevImgArr[idx] * (1 - 0.1) + imgArr[idx] * 0.1;
			}
		}
		
		int maxSize = bfs();

		bitmap->UnlockBits(bitmapData);
		delete bitmapData;
	
		if(minimum && (maximum || maxSize > resizeWidth * resizeHeight * 0.01)){//save image
			CLSID clsid;
			GetEncoderClsid(L"image/jpeg", &clsid);
			sprintf(buf, "%s.jpeg", filename);
			ConvertCtoWC(buf, WCbuf);
			origin->Save(WCbuf, &clsid, NULL);
			if(debug){
				sprintf(buf, "%s(diff).jpeg", filename);
				ConvertCtoWC(buf, WCbuf);
				bitmap->Save(WCbuf, &clsid, NULL);
			}
			save = true;
		}

		delete origin;
		delete bitmap;
		DeleteObject(memdc);
		DeleteObject(membit);
		::ReleaseDC(NULL, scrdc);
	}
	GdiplusShutdown(gdiplusToken);

	return save;
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

void CALLBACK routine(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime){
	char filename[128];
	char foldername[128];
	char buf[256];
	time_t curr_time;
	struct tm *curr_tm, *tmp_tm;

	time(&curr_time);
	curr_tm = localtime(&curr_time);
	strftime(foldername, sizeof(foldername), "%y%m%d", curr_tm);
	sprintf(buf, "%s\\%s", path, foldername);
	if(!dirExists(buf)){
		mkdir(buf);
	
		time_t tmp_time = curr_time - 86400 * deleteDelay;
		tmp_tm = localtime(&tmp_time);
		strftime(foldername, sizeof(foldername), "%y%m%d", tmp_tm);
		sprintf(buf, "%s\\%s", path, foldername);
		if(dirExists(buf)){
			sprintf(buf, "rmdir /s /q %s\\%s", path, foldername);//delete directory
			system(buf);
		}
	}
	strftime(filename, sizeof(filename), "%H-%M-%S", curr_tm);
	sprintf(buf, "%s\\%s\\%s", path, foldername, filename);
	if(gdiscreen(buf, difftime(curr_time, prev_time) >= maximumSaveDelay, difftime(curr_time, prev_time) >= minimumSaveDelay))
		time(&prev_time);
}

void exitHandler(int signum){
	MessageBox(NULL, "program is terminated", "autoScreenCapture", MB_OK|MB_ICONERROR);	
}


void init(){
	cmd["help"] = CMD_HELP;//init command map
	cmd["quit"] = CMD_QUIT;
	cmd["setDelay"] = CMD_SETDELAY;
	cmd["setTimer"] = CMD_SETTIMER;
	cmd["setDebug"] = CMD_SETDEBUG;
	cmd["clear"] = CMD_CLEAR;
	cmd["info"] = CMD_INFO;

	imgArr = new BYTE[resizeWidth * resizeHeight];
	prevImgArr = new BYTE[resizeWidth * resizeHeight];
	diffImgArr = new BYTE[resizeWidth * resizeHeight];
	memset(prevImgArr, 0, sizeof(BYTE) * resizeWidth * resizeHeight);

	if(!dirExists(path))
		mkdir(path);

	AllocConsole();//init concole
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	initSetting();
	
	SetTimer(NULL, 0, (int)1000/timerDelay, (TIMERPROC)&routine);

	signal(SIGABRT, exitHandler);

	hThread = (HANDLE)_beginthreadex(NULL, 0, keyboardInput, NULL, 0, (unsigned*)&threadID);
}

void initSetting(){
	FILE *pFile;
	char tmp;
	
	pFile = fopen("setting", "r");
	if(pFile != NULL){
		fscanf(pFile, "%10d%10d%10d%c", &timerDelay, &minimumSaveDelay, &maximumSaveDelay, &tmp);
		debug = (tmp=='T')?true:false;
	}
	else{//make setting file
		fclose(pFile);
		pFile = fopen("setting", "w");
		fprintf(pFile, "%10d%10d%10dF", timerDelay, minimumSaveDelay, maximumSaveDelay);
	}
	
	fclose(pFile);
}

unsigned int WINAPI keyboardInput(void *args){//keyboard input thread
	char input[256];
	char buf[256];
	int inputIdx, bufIdx;
	while(true){
		cin.getline(input, sizeof(input));
		sscanf(input, "%s", buf);
		switch(cmd[buf]){
			case CMD_HELP:
				printf("help : show command\n");
				printf("quit : quit program\n");
				printf("setDelay 0 0 : set minimum, maximum save delay\n");
				printf("setTimer 0 : set timer delay\n");
				printf("setDebug (T|F) : set debug mode\n");
				printf("clear : delete save image\n");
				printf("info : show information\n");
				break;
			case CMD_QUIT:
				return 0;
			case CMD_SETDELAY:{
				int minimum, maximum;
				if(sscanf(input, "%*s %d %d", &minimum, &maximum) != 2){
					printf("input error\n");
					break;
				}
				if(minimum == 0)
					minimum = minimumSaveDelay;
				if(maximum == 0)
					maximum = maximumSaveDelay;
				if(minimum > maximum){
					printf("error : (%d)minimumSaveDelay > (%d)maximumSaveDelay\n", minimum, maximum);
				}
				else{
					minimumSaveDelay = minimum;
					maximumSaveDelay = maximum;
				}
				printf("minimumSaveDelay : %dsec, maximumSaveDelay : %dsec\n", minimum, maximum); 
				FILE *pFile;
				pFile = fopen("setting", "r+");
				fseek(pFile, 10, SEEK_SET);
				fprintf(pFile, "%10d%10d", minimumSaveDelay, maximumSaveDelay);
				fclose(pFile);
				break;}
			case CMD_SETTIMER:{
				int timer;
				if(sscanf(input, "%*s %d", &timer) != 1){
					printf("input error\n");
					break; 
				}
				if(timer < 1)
					timer = 1;
				else if(timer > 20)
					timer = 20;
				timerDelay = timer;
				printf("timerDelay : %dfps\n", timerDelay);
				FILE *pFile;
				pFile = fopen("setting", "r+");
				fseek(pFile, 0, SEEK_SET);
				fprintf(pFile, "%10d", timerDelay);
				fclose(pFile);
				break;}
			case CMD_SETDEBUG:{
				char tmp;
				if(sscanf(input, "%*s %c", &tmp) != 1 || (tmp != 'T' && tmp != 'F')){
					printf("input error\n");
					break;
				}
				debug = (tmp=='T')?true:false;
				printf("Debug : %c", tmp);
				FILE *pFile;
				pFile = fopen("setting", "r+");
				fseek(pFile, 30, SEEK_SET);
				fprintf(pFile, "%c", tmp);
				fclose(pFile);	
				break;}
			case CMD_CLEAR:
				if(dirExists(path)){
					char sysBuf[256];
					sprintf(sysBuf, "rmdir /s /q %s", path);//delete directory
					system(sysBuf);
					mkdir(path);
					printf("saved image has been deleted\n");
				}
				break;
			case CMD_INFO:
				printf("Width X Height : %d %d\n", Width, Height);
				printf("screen capture : %dfps\n", timerDelay);
				printf("save delay : %d ~ %d(sec)\n", minimumSaveDelay, maximumSaveDelay);
				break;
			default:
				printf("command not found\nhelp : show command\n");
				break;
		}
	}
}

int bfs(){
	int *arr = new int[resizeHeight * resizeWidth];
	int cnt = 0, size, maxSize = 0;
	memset(arr, 0, sizeof(BYTE) * resizeHeight * resizeWidth);
	stack<int> s;
	for(int j = 0;j < resizeHeight; j++){
		for(int i = 0;i < resizeWidth; i++){
			int idx = j * resizeWidth + i;
			if(arr[idx] == 0){
				if(diffImgArr[idx] < 10){
					arr[idx] = -1;
					continue;	
				}
				size = 0;
				arr[idx] = ++cnt;
				s.push(idx);
				while(!s.empty()){
					idx = s.top();
					s.pop();
					if(diffImgArr[idx] < 10)continue;
					size++;
					if(idx >= 1 && arr[idx - 1] == 0){
						arr[idx - 1] = cnt;
						s.push(idx - 1);
					}
					if(idx + 1 < resizeHeight * resizeWidth && arr[idx + 1] == 0){
						arr[idx + 1] = cnt;
						s.push(idx + 1);
					}
					if(idx >= resizeWidth && arr[idx - resizeWidth] == 0){
						arr[idx - resizeWidth] = cnt;
						s.push(idx - resizeWidth);
					}
					if(idx + resizeWidth < resizeHeight * resizeWidth && arr[idx + resizeWidth] == 0){
						arr[idx + resizeWidth] = cnt;
						s.push(idx + resizeWidth);
					}
				}
				if(maxSize < size)
					maxSize = size;
			}
		}
	}
	delete[] arr;
	return maxSize;
}
