#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_LARG 1000
#define WINDOW_ALT 600
#define NUM_MONSTROS 3

typedef struct {
    SDL_Rect ret;
    int veloc;
    int velPulo;
    int velY;
    bool pulando;
    bool abaixando;
} Character;

typedef struct {
    SDL_Rect ret;
    bool ativo;
} Monstro;

bool colisao(SDL_Rect a, SDL_Rect b) {
    return (a.x < b.x + b.w &&
            a.x + a.w > b.x &&
            a.y < b.y + b.h &&
            a.y + a.h > b.y);
}

void desenharLosango(SDL_Renderer* renderer, int x, int y, int tam) {
    SDL_Point p[5] = {
        {x, y - tam},
        {x + tam, y},
        {x, y + tam},
        {x - tam, y},
        {x, y - tam}
    };
    SDL_RenderDrawLines(renderer, p, 5);
    SDL_RenderDrawLines(renderer, p, 5);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    srand(time(NULL));

    SDL_Window* window = SDL_CreateWindow(
        "Luke's Adventure",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_LARG, WINDOW_ALT, 0
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    Character Luke = {{100, WINDOW_ALT - 150, 50, 100}, 5, -15, 0, false, false};

    Monstro monstros[NUM_MONSTROS];
    for (int i = 0; i < NUM_MONSTROS; i++) {
        monstros[i].ret.x = 300 + i * 150;
        monstros[i].ret.y = WINDOW_ALT - 150;
        monstros[i].ret.w = 40;
        monstros[i].ret.h = 60;
        monstros[i].ativo = true;
    }

    bool fase2 = false, fase3 = false;
    int monstrosPegos = 0;
    bool portalVisivel = false;
    SDL_Rect portalLosango = {WINDOW_LARG - 100, WINDOW_ALT - 100, 40, 40};

    bool esqPress = false, dirPress = false, baixoPress = false, puloPress = false, cimaPress = false;
    bool rodando = true;
    SDL_Event e;

    while (rodando) {
        // Entrada de eventos
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) rodando = false;

            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_LEFT:  esqPress = true; break;
                    case SDL_SCANCODE_RIGHT: dirPress = true; break;
                    case SDL_SCANCODE_DOWN:  baixoPress = true; break;
                    case SDL_SCANCODE_UP:    cimaPress = true; break;
                    case SDL_SCANCODE_SPACE: puloPress = true; break;
                    default: break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_LEFT:  esqPress = false; break;
                    case SDL_SCANCODE_RIGHT: dirPress = false; break;
                    case SDL_SCANCODE_DOWN:  baixoPress = false; break;
                    case SDL_SCANCODE_UP:    cimaPress = false; break;
                    case SDL_SCANCODE_SPACE: puloPress = false; break;
                    default: break;
                }
            }
        }

        // --- Lógica das fases ---
        if (!fase2 && !fase3) {
            // FASE 1: andar e pegar monstros fixos
            if (esqPress) Luke.ret.x -= Luke.veloc;
            if (dirPress) Luke.ret.x += Luke.veloc;

            if (puloPress && !Luke.pulando && !Luke.abaixando) {
                Luke.pulando = true;
                Luke.velY = Luke.velPulo;
            }

            if (Luke.pulando) {
                Luke.ret.y += Luke.velY;
                Luke.velY += 1;
                if (Luke.ret.y >= WINDOW_ALT - 150) {
                    Luke.ret.y = WINDOW_ALT - 150;
                    Luke.pulando = false;
                }
            }

            for (int i = 0; i < NUM_MONSTROS; i++) {
                if (monstros[i].ativo && colisao(Luke.ret, monstros[i].ret)) {
                    monstros[i].ativo = false;
                    monstrosPegos++;
                }
            }

            if (monstrosPegos >= 3) portalVisivel = true;

            if (portalVisivel && colisao(Luke.ret, portalLosango)) {
                fase2 = true;
                Luke.ret.x = 100;
                Luke.ret.y = WINDOW_ALT - 150;
            }

            // Renderização Fase 1
            SDL_SetRenderDrawColor(renderer, 6, 0, 53, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &Luke.ret);

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            for (int i = 0; i < NUM_MONSTROS; i++)
                if (monstros[i].ativo)
                    SDL_RenderFillRect(renderer, &monstros[i].ret);

            if (portalVisivel) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                desenharLosango(renderer, portalLosango.x, portalLosango.y, 30);
            }
            SDL_RenderPresent(renderer);
        }

        else if (fase2 && !fase3) {
            // FASE 2: monstros caindo do topo
            static Monstro monstros2[NUM_MONSTROS];
            static bool inicializado = false;
            if (!inicializado) {
                for (int i = 0; i < NUM_MONSTROS; i++) {
                    monstros2[i].ret.x = rand() % (WINDOW_LARG - 40);
                    monstros2[i].ret.y = rand() % 200 - 200;
                    monstros2[i].ret.w = 40;
                    monstros2[i].ret.h = 60;
                    monstros2[i].ativo = true;
                }
                monstrosPegos = 0;
                portalVisivel = false;
                inicializado = true;
            }

            // Movimento do Luke
            if (esqPress) Luke.ret.x -= Luke.veloc;
            if (dirPress) Luke.ret.x += Luke.veloc;
            if (cimaPress) Luke.ret.y -= Luke.veloc;
            if (baixoPress) Luke.ret.y += Luke.veloc;

            if (puloPress && !Luke.pulando) {
                Luke.pulando = true;
                Luke.velY = Luke.velPulo;
            }
            if (Luke.pulando) {
                Luke.ret.y += Luke.velY;
                Luke.velY += 1;
                if (Luke.ret.y >= WINDOW_ALT - 150) {
                    Luke.ret.y = WINDOW_ALT - 150;
                    Luke.pulando = false;
                }
            }

            // Queda dos monstros
            for (int i = 0; i < NUM_MONSTROS; i++) {
                if (monstros2[i].ativo) {
                    monstros2[i].ret.y += 5;
                    if (monstros2[i].ret.y > WINDOW_ALT) {
                        monstros2[i].ret.y = -50;
                        monstros2[i].ret.x = rand() % (WINDOW_LARG - 40);
                    }
                    if (colisao(Luke.ret, monstros2[i].ret)) {
                        monstros2[i].ativo = false;
                        monstrosPegos++;
                    }
                }
            }

            if (monstrosPegos >= 3) portalVisivel = true;
            if (portalVisivel && colisao(Luke.ret, portalLosango)) {
                fase3 = true;
            }

            // Renderização Fase 2
            SDL_SetRenderDrawColor(renderer, 0, 0, 80, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &Luke.ret);

            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            for (int i = 0; i < NUM_MONSTROS; i++)
                if (monstros2[i].ativo)
                    SDL_RenderFillRect(renderer, &monstros2[i].ret);

            if (portalVisivel) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                desenharLosango(renderer, portalLosango.x, portalLosango.y, 30);
            }
            SDL_RenderPresent(renderer);
        }

        else if (fase3) {
            // FASE 3: só fundo diferente para indicar chegada
            SDL_SetRenderDrawColor(renderer, 0, 150, 150, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
