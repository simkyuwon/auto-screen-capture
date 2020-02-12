autoScreenCapture_exe :
	x86_64-w64-mingw32-g++ -mwindows main.cpp -o autoScreenCapture.exe -lgdiplus -static

clean :
	rm autoScreenCapture.exe
