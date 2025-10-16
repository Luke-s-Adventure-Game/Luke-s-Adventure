#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL_ttf.h>

#define WINDOW_LARG 1000
#define WINDOW_ALT 600
#define GRAVIDADE 1
#define MAX_OBSTACULOS 10
#define MAX_INIMIGOS 20
#define MAX_CORACOES 5
#define MAX_VIDAS 5

// === NOVO ===: constantes para o lançador de rede
#define MAX_REDES 10
#define REDE_W 24
#define REDE_H 12

typedef struct {
    SDL_Rect ret;
    bool solido;
    bool plataforma;
} Obstaculo;

typedef struct {
    SDL_Rect ret;
    int veloc;
    int velPulo;
    int velY;
    bool pulando;
    bool abaixando;
    int coracoes;
    int vidas;
    int invencivel;
} Character;

typedef struct {
    SDL_Rect ret;
    int dir;
    int veloc;
    int velY;
    bool ativo;
} Inimigo;

// === NOVO ===: estrutura do projétil (rede)
typedef struct {
    SDL_Rect ret;
    int velocidade; // pode ser negativa para ir para a esquerda
    bool ativo;
} Rede;

bool colidem(SDL_Rect a, SDL_Rect b) {
    return SDL_HasIntersection(&a, &b);
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Erro ao inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Luke's Adventure",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_LARG, WINDOW_ALT, 0);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Erro ao criar renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("Erro TTF_Init: %s\n", TTF_GetError());
    }
    TTF_Font *font = TTF_OpenFont("assets/DejaVuSans.ttf", 20);
    if (!font) {
        printf("Erro ao abrir fonte: %s\n", TTF_GetError());
    }

    SDL_Color textColor = {255, 255, 255, 255};

    srand((unsigned)time(NULL));

    // --- Personagem ---
    Character Luke;
    Luke.ret.w = 50;
    Luke.ret.h = 100;
    Luke.ret.x = 100;
    Luke.ret.y = WINDOW_ALT - 150;
    Luke.veloc = 5;
    Luke.velPulo = -15;
    Luke.velY = 0;
    Luke.pulando = false;
    Luke.abaixando = false;
    Luke.coracoes = MAX_CORACOES;
    Luke.vidas = MAX_VIDAS;
    Luke.invencivel = 0;

    // --- Controle de fase ---
    int faseAtual = 1;
    int cameraX = 0;
    int faseLargura = 3000;

    // --- Obstáculos fase 1 ---
    Obstaculo obstaculos[MAX_OBSTACULOS];
    int qtdObs = 0;
    obstaculos[qtdObs++] = (Obstaculo){{0, WINDOW_ALT - 50, faseLargura, 50}, true, false};
    obstaculos[qtdObs++] = (Obstaculo){{600, WINDOW_ALT - 120, 100, 20}, true, true};
    obstaculos[qtdObs++] = (Obstaculo){{1000, WINDOW_ALT - 200, 150, 20}, true, true};
    obstaculos[qtdObs++] = (Obstaculo){{1300, WINDOW_ALT - 280, 150, 20}, true, true};
    obstaculos[qtdObs++] = (Obstaculo){{2000, WINDOW_ALT - 120, 100, 20}, true, true};
    
    SDL_Rect porta = {2800, WINDOW_ALT - 150, 50, 100};

    // --- Inimigos ---
    Inimigo inimigos[MAX_INIMIGOS] = {0};
    int qtdInimigos = 3;
    inimigos[0] = (Inimigo){{800, WINDOW_ALT - 150, 50, 50}, -1, 2, 0, true};
    inimigos[1] = (Inimigo){{1600, WINDOW_ALT - 150, 50, 50}, 1, 3, 0, true};
    inimigos[2] = (Inimigo){{2300, WINDOW_ALT - 150, 50, 50}, -1, 2, 0, true};

    Uint32 ultimoSpawn = SDL_GetTicks();

    // === NOVO ===: redes (projéteis)
    Rede redes[MAX_REDES];
    for (int i = 0; i < MAX_REDES; i++) {
        redes[i].ativo = false;
        redes[i].ret = (SDL_Rect){0,0,REDE_W,REDE_H};
        redes[i].velocidade = 0;
    }

    // === NOVO ===: contador de inimigos destruídos
    int contadorInimigosMortos = 0;

    //---Estados de tecla---
    bool esqPress = false, dirPress = false, baixoPress = false, puloPress = false;
    bool rodando = true;
    SDL_Event e;

    // --- Loop Principal ---
    while (rodando) {
        while (SDL_WaitEventTimeout(&e, 16)) {
            if (e.type == SDL_QUIT)
                rodando = false;

            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_LEFT: esqPress = true; break;
                    case SDL_SCANCODE_RIGHT: dirPress = true; break;
                    case SDL_SCANCODE_DOWN: baixoPress = true; break;
                    case SDL_SCANCODE_SPACE: puloPress = true; break;
                    // === NOVO ===: disparo da rede com tecla X
                    case SDL_SCANCODE_X: {
                        // determina direção de disparo: -1 (esq) ou 1 (dir). Se parado, dispara para a direita.
                        int facing = (dirPress ? 1 : (esqPress ? -1 : 1));
                        // busca slot livre
                        for (int idx = 0; idx < MAX_REDES; idx++) {
                            if (!redes[idx].ativo) {
                                // posiciona a rede na frente do personagem
                                if (facing > 0) {
                                    redes[idx].ret.x = Luke.ret.x + Luke.ret.w;
                                } else {
                                    redes[idx].ret.x = Luke.ret.x - REDE_W;
                                }
                                redes[idx].ret.y = Luke.ret.y + Luke.ret.h/2 - REDE_H/2;
                                redes[idx].velocidade = 12 * facing;
                                redes[idx].ativo = true;
                                break;
                            }
                        }
                    } break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_LEFT: esqPress = false; break;
                    case SDL_SCANCODE_RIGHT: dirPress = false; break;
                    case SDL_SCANCODE_DOWN: baixoPress = false; break;
                    case SDL_SCANCODE_SPACE: puloPress = false; break;
                }
            }
        }

        // --- Movimento horizontal ---
        int movimentoX = (dirPress - esqPress) * Luke.veloc;
        Luke.ret.x += movimentoX;

        // --- Colisão lateral ---
        for (int i = 0; i < qtdObs && faseAtual == 1; i++) {
            if (!obstaculos[i].solido) continue;
            if (colidem(Luke.ret, obstaculos[i].ret)) {
                if (movimentoX > 0) Luke.ret.x = obstaculos[i].ret.x - Luke.ret.w;
                else if (movimentoX < 0) Luke.ret.x = obstaculos[i].ret.x + obstaculos[i].ret.w;
            }
        }

        // --- Gravidade e pulo ---
        if (puloPress && !Luke.pulando && !Luke.abaixando) {
            Luke.pulando = true;
            Luke.velY = Luke.velPulo;
        }

        Luke.ret.y += Luke.velY;
        Luke.velY += GRAVIDADE;

        bool noChao = false;
        if (faseAtual == 1) {
            for (int i = 0; i < qtdObs; i++) {
                if (!obstaculos[i].solido) continue;
                if (colidem(Luke.ret, obstaculos[i].ret)) {
                    if (Luke.velY > 0 && Luke.ret.y + Luke.ret.h > obstaculos[i].ret.y) {
                        Luke.ret.y = obstaculos[i].ret.y - Luke.ret.h;
                        Luke.velY = 0;
                        Luke.pulando = false;
                        noChao = true;
                    }
                }
            }
        } else {
            if (Luke.ret.y + Luke.ret.h >= WINDOW_ALT - 50) {
                Luke.ret.y = WINDOW_ALT - 150;
                Luke.velY = 0;
                Luke.pulando = false;
                noChao = true;
            }
        }

        if (!noChao && Luke.velY == 0)
            Luke.pulando = true;

        // --- Abaixar ---
        if (baixoPress && !Luke.pulando) {
            if (!Luke.abaixando) {
                Luke.abaixando = true;
                Luke.ret.h = 50;
                Luke.ret.y += 50;
            }
        } else if (Luke.abaixando) {
            Luke.abaixando = false;
            Luke.ret.y -= 50;
            Luke.ret.h = 100;
        }

        // --- Atualiza inimigos ---
        if (faseAtual == 1) {
            for (int i = 0; i < qtdInimigos; i++) {
                inimigos[i].ret.x += inimigos[i].dir * inimigos[i].veloc;
                if (inimigos[i].ret.x < 500 || inimigos[i].ret.x > 2500)
                    inimigos[i].dir *= -1;
            }
        } else if (faseAtual == 2) {
            // Spawn de inimigos caindo
            Uint32 agora = SDL_GetTicks();
            if (agora - ultimoSpawn > 1500) {
                for (int i = 0; i < MAX_INIMIGOS; i++) {
                    if (!inimigos[i].ativo) {
                        inimigos[i].ret = (SDL_Rect){rand() % (WINDOW_LARG - 50), -50, 40, 40};
                        inimigos[i].velY = 3 + rand() % 4;
                        inimigos[i].ativo = true;
                        break;
                    }
                }
                ultimoSpawn = agora;
            }

            // Atualiza queda
            for (int i = 0; i < MAX_INIMIGOS; i++) {
                if (!inimigos[i].ativo) continue;
                inimigos[i].ret.y += inimigos[i].velY;
                if (inimigos[i].ret.y > WINDOW_ALT)
                    inimigos[i].ativo = false;
            }
        }

        // === NOVO ===: atualizar redes (movimentação + desativar quando fora da tela)
        for (int r = 0; r < MAX_REDES; r++) {
            if (!redes[r].ativo) continue;
            redes[r].ret.x += redes[r].velocidade;
            if (redes[r].ret.x > WINDOW_LARG || (redes[r].ret.x + redes[r].ret.w) < 0) {
                redes[r].ativo = false;
            }
        }

        // === NOVO ===: colisão entre redes e inimigos
        for (int r = 0; r < MAX_REDES; r++) {
            if (!redes[r].ativo) continue;
            for (int i = 0; i < MAX_INIMIGOS; i++) {
                if (!inimigos[i].ativo) continue;
                if (colidem(redes[r].ret, inimigos[i].ret)) {
                    // destrói inimigo e rede
                    inimigos[i].ativo = false;
                    redes[r].ativo = false;

                    // incrementa contador total de inimigos destruídos
                    contadorInimigosMortos++;

                    // só concede vida quando na FASE 2 e a cada 2 inimigos mortos
                    if (faseAtual == 2 && (contadorInimigosMortos % 2) == 0) {
                        if (Luke.vidas < MAX_VIDAS) {
                            Luke.vidas++;
                        }
                    }

                    // quebra para evitar múltiplas colisões com a mesma rede
                    break;
                }
            }
        }

        // --- Dano por colisão (player) ---
        if (Luke.invencivel > 0) Luke.invencivel--;
        for (int i = 0; i < MAX_INIMIGOS; i++) {
            if (inimigos[i].ativo && colidem(Luke.ret, inimigos[i].ret) && Luke.invencivel == 0) {
                Luke.coracoes--;
                Luke.invencivel = 60;
                if (Luke.coracoes <= 0) {
                    Luke.vidas--;
                    Luke.coracoes = MAX_CORACOES;
                    Luke.ret.x = 100;
                    Luke.ret.y = WINDOW_ALT - 150;
                }
                if (Luke.vidas <= 0) {
                    printf("Game Over!\n");
                    rodando = false;
                }
            }
        }

        // --- Câmera ---
        cameraX = Luke.ret.x - WINDOW_LARG / 2 + Luke.ret.w / 2;
        if (cameraX < 0) cameraX = 0;
        if (cameraX > faseLargura - WINDOW_LARG)
            cameraX = faseLargura - WINDOW_LARG;

        // --- Renderização ---
        SDL_SetRenderDrawColor(renderer, 20, 20, 70, 255);
        SDL_RenderClear(renderer);

        // Fundo
        SDL_SetRenderDrawColor(renderer, 40, 40, 120, 255);
        SDL_Rect fundo = {0, 0, WINDOW_LARG, WINDOW_ALT};
        SDL_RenderFillRect(renderer, &fundo);

        // Fase 1: obstáculos
        if (faseAtual == 1) {
            for (int i = 0; i < qtdObs; i++) {
                SDL_Rect obsTela = obstaculos[i].ret;
                obsTela.x -= cameraX;
                SDL_SetRenderDrawColor(renderer, 100, 60, 20, 255);
                SDL_RenderFillRect(renderer, &obsTela);
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 70, 100, 255);
            SDL_Rect chao = {0, WINDOW_ALT - 50, WINDOW_LARG, 50};
            SDL_RenderFillRect(renderer, &chao);
        }

        // Inimigos
        SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
        for (int i = 0; i < MAX_INIMIGOS; i++) {
            if (!inimigos[i].ativo) continue;
            SDL_Rect inimTela = inimigos[i].ret;
            inimTela.x -= (faseAtual == 1 ? cameraX : 0);
            SDL_RenderFillRect(renderer, &inimTela);
        }

        // === NOVO ===: desenhar redes por cima (projéteis)
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        for (int r = 0; r < MAX_REDES; r++) {
            if (!redes[r].ativo) continue;
            SDL_Rect redeTela = redes[r].ret;
            // em fase 1, ajustar posição em relação à câmera se necessário (se as redes usam coordenadas world)
            if (faseAtual == 1) redeTela.x -= cameraX;
            SDL_RenderFillRect(renderer, &redeTela);
        }

        // Porta (apenas fase 1)
        if (faseAtual == 1) {
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
            SDL_Rect portaTela = porta;
            portaTela.x -= cameraX;
            SDL_RenderFillRect(renderer, &portaTela);
            
            // Transição de fase
            SDL_Rect lukeTela = {Luke.ret.x - cameraX, Luke.ret.y, Luke.ret.w, Luke.ret.h};
            if (SDL_HasIntersection(&lukeTela, &portaTela)) {
                printf("Fase 1 concluída! Indo para fase 2...\n");
                faseAtual = 2;
                Luke.ret.x = 100;
                Luke.ret.y = WINDOW_ALT - 150;
                Luke.velY = 0;
                cameraX = 0;
                for (int i = 0; i < MAX_INIMIGOS; i++) inimigos[i].ativo = false;
                // opcional: resetar contador se desejar (a lógica atual mantém o acumulado)
                // contadorInimigosMortos = 0;
            }
        }

        // Luke
        if (Luke.invencivel % 10 < 5)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        else
            SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
        SDL_Rect lukeTela = {Luke.ret.x - (faseAtual == 1 ? cameraX : 0), Luke.ret.y, Luke.ret.w, Luke.ret.h};
        SDL_RenderFillRect(renderer, &lukeTela);

        // --- HUD (tela de alerta)---
        int margem = 20;
        int coracaoTam = 25;
        for (int i = 0; i < MAX_CORACOES; i++) {
            if (i < Luke.coracoes)
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            else
                SDL_SetRenderDrawColor(renderer, 60, 0, 0, 255);
            SDL_Rect c = {margem + i * (coracaoTam + 5), margem, coracaoTam, coracaoTam};
            SDL_RenderFillRect(renderer, &c);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect vidasRect = {margem, margem + 40, 20 * Luke.vidas, 10};
        SDL_RenderFillRect(renderer, &vidasRect);

        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
        SDL_Rect faseRect = {WINDOW_LARG - 150, margem, 100, 20};
        SDL_RenderFillRect(renderer, &faseRect);

        // Texto numérico ao lado dos corações
        if (font) {
            char buf[32];
            SDL_Surface *surf = NULL;
            SDL_Texture *tex = NULL;
            int tw, th;

            // contador de corações (ex: "x3")
            snprintf(buf, sizeof(buf), "x%d", Luke.coracoes);
            surf = TTF_RenderText_Solid(font, buf, textColor);
            tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
            SDL_Rect dstCor = { margem + MAX_CORACOES * (coracaoTam + 5) + 10, margem, tw, th };
            SDL_RenderCopy(renderer, tex, NULL, &dstCor);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);

            // contador de vidas (ex: "Vidas: 2")
            snprintf(buf, sizeof(buf), "Vidas: %d", Luke.vidas);
            surf = TTF_RenderText_Solid(font, buf, textColor);
            tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
            SDL_Rect dstVidas = { margem, margem + 60, tw, th };
            SDL_RenderCopy(renderer, tex, NULL, &dstVidas);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}





