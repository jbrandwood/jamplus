#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char** argv)
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Event event;
    int done = 0;

    SDL_Init(SDL_INIT_VIDEO);

    //const SDL_DisplayMode* displayMode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());

    //window = SDL_CreateWindow("SDL Tutorial", displayMode->w, displayMode->h, 0);
    window = SDL_CreateWindow("SDL Tutorial", 640, 480, SDL_WINDOW_FULLSCREEN);
    renderer = SDL_CreateRenderer(window, NULL);

    while (!done) {
        SDL_PumpEvents();
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    done = 1;
                    break;

                case SDL_EVENT_DID_ENTER_FOREGROUND:
                    SDL_Log("SDL_APP_DID_ENTER_FOREGROUND");
                    break;

                case SDL_EVENT_DID_ENTER_BACKGROUND:
                    SDL_Log("SDL_EVENT_DID_ENTER_BACKGROUND");
                    break;

                case SDL_EVENT_LOW_MEMORY:
                    SDL_Log("SDL_EVENT_LOW_MEMORY");
                    break;

                case SDL_EVENT_TERMINATING:
                    SDL_Log("SDL_EVENT_TERMINATING");
                    break;

                case SDL_EVENT_WILL_ENTER_BACKGROUND:
                    SDL_Log("SDL_EVENT_WILL_ENTER_BACKGROUND");
                    break;

                case SDL_EVENT_WILL_ENTER_FOREGROUND:
                    SDL_Log("SDL_EVENT_WILL_ENTER_FOREGROUND");
                    break;

                case SDL_EVENT_FINGER_MOTION:
                    SDL_Log("SDL_EVENT_FINGER_MOTION");
                    break;

                case SDL_EVENT_FINGER_DOWN:
                    SDL_Log("SDL_EVENT_FINGER_DOWN");
                    break;

                case SDL_EVENT_FINGER_UP:
                    SDL_Log("SDL_EVENT_FINGER_UP");
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    //SDL_Surface* screenSurface;
    //screenSurface = SDL_GetWindowSurface(window);
    //SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0xFF, 0xFF));
    //SDL_UpdateWindowSurface(window);
    SDL_DestroyWindow( window );
    SDL_Quit();

    return 0;
}
