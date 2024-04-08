#pragma once
#ifndef Artist
#include "Artist.h"
#endif
#ifndef vector
#include <vector>
#endif
class Controller {
public:
	static bool quit;
	//Keeps track of mouse Pos
	static int mouseX, mouseY;
	static bool mouseMoved;
	static Artist::pos lastMousePos;
	static Uint32 SDLMouseStates;
	static std::vector<int> keyboardStates;
	static std::vector<int> mouseStates;
	bool loadController();
	//Checks for inputs from the user
	void controller();

	//TEMP
	static Artist::point spin;
};
