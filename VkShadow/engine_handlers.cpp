#include "engine.h"

void Engine::handle_minimize() {
	minimized = true;
	LOG(1, "Minimized.");
}

void Engine::handle_restore() {
	minimized = false;
	LOG(1, "Restored");
}

void Engine::handle_keypress(int key, int scanecode, int action, int mods) {
//TODO
}

void Engine::handle_cursor_pos(double xpos, double ypos) {
//TODO
}
