#include "winbase.h"

extern "C" int main(int argc, char** argv, char** envp);

__declspec(dllexport) extern "C" int mainCRTStartup() {
	return main(0, nullptr, nullptr);
}
