// Probe: list SDL joysticks + gamepad classification (uses SDK's SDL3).
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_gamepad.h>
#include <stdio.h>

int main(void) {
  if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_UpdateJoysticks();
  int n = 0;
  SDL_JoystickID* ids = SDL_GetJoysticks(&n);
  printf("joysticks: %d\n", n);
  for (int i = 0; i < n; i++) {
    const char* name = SDL_GetJoystickNameForID(ids[i]);
    SDL_GUID g = SDL_GetJoystickGUIDForID(ids[i]);
    char guid[64] = {0};
    SDL_GUIDToString(g, guid, sizeof(guid));
    printf("[%d] id=%d name=\"%s\" guid=%s is_gamepad=%d\n", i,
           (int)ids[i], name ? name : "?", guid,
           SDL_IsGamepad(ids[i]) ? 1 : 0);
    if (SDL_IsGamepad(ids[i])) {
      SDL_Gamepad* gp = SDL_OpenGamepad(ids[i]);
      printf("     opened=%d type=%d vendor=0x%04x product=0x%04x mapping=%s\n",
             gp ? 1 : 0, gp ? (int)SDL_GetGamepadType(gp) : -1,
             gp ? SDL_GetGamepadVendor(gp) : 0,
             gp ? SDL_GetGamepadProduct(gp) : 0,
             gp && SDL_GetGamepadMapping(gp) ? "yes" : "no");
      if (gp) SDL_CloseGamepad(gp);
    }
  }
  SDL_free(ids);
  SDL_Quit();
  return 0;
}
