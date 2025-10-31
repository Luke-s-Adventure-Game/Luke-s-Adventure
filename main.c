#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_LARG 1000
#define WINDOW_ALT 600
#define GRAVIDADE 1
#define MAX_OBSTACULOS 10
#define MAX_INIMIGOS 20
#define MAX_CORACOES 5
#define MAX_VIDAS 5

// Constantes para o lançador de rede
#define MAX_REDES 50
#define REDE_W 24
#define REDE_H 12

// Duração da transição em milissegundos (5s)
#define TRANSITION_MS 5000
// Fade-in / hold / fade-out em ms (1s / 3s / 1s)
#define FADE_IN_MS 1000
#define HOLD_MS 3000
#define FADE_OUT_MS 1000

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
    int facing; //direita = 1, esquerda = -1
} Character;

typedef struct {
    SDL_Rect ret;
    int dir;
    int veloc;
    int velY;
    bool ativo;
} Inimigo;

// Estrutura do projétil (rede)
typedef struct {
    SDL_Rect ret;
    int velocidade; // pode ser negativa para ir para a esquerda
    bool ativo;
} Rede;

bool colidem(SDL_Rect a, SDL_Rect b) {
    return SDL_HasIntersection(&a, &b);
}

bool podeLevantar(Character c, Obstaculo* obstaculos, int qtdObs) {
    if (!c.abaixando) return true;

    SDL_Rect hitbox = c.ret;
    int dif = 50; // diferença entre abaixado (50) e em pé (100)
    hitbox.y -= dif; // simula levantar
    hitbox.h += dif;

    for (int i = 0; i < qtdObs; i++) {
        if (!obstaculos[i].solido) continue;
        if (colidem(hitbox, obstaculos[i].ret)) return false;
    }
    return true;
}

// Funções utilitárias
static SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path) {
    SDL_Texture* tex = IMG_LoadTexture(renderer, path);
    if (!tex) {
        fprintf(stderr, "Erro ao carregar '%s': %s\n", path, IMG_GetError());
    }
    return tex;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("Erro ao inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0) {
        // não fatal; só log
        SDL_Log("Aviso: IMG_Init falhou: %s", IMG_GetError());
    }

    if (TTF_Init() == -1) {
        SDL_Log("Aviso: TTF_Init falhou: %s", TTF_GetError());
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Aviso: Mix_OpenAudio falhou: %s", Mix_GetError());
    }

    SDL_Window* window = SDL_CreateWindow("Luke's Adventure - Transição",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_LARG, WINDOW_ALT, 0);
    if (!window) {
        printf("Erro ao criar janela: %s\n", SDL_GetError());
        goto cleanup_sdl;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Erro ao criar renderer: %s\n", SDL_GetError());
        goto cleanup_window;
    }

    // Carregar sprites (usuário confirmou que existem)
    SDL_Texture* texLuke   = loadTexture(renderer, "assets/luke.png");
    SDL_Texture* texAranha = loadTexture(renderer, "assets/aranha.png");
    SDL_Texture* texRede   = loadTexture(renderer, "assets/rede.png");

    // Se alguma textura não carregar, o código seguirá, mas usará retângulos de fallback
    bool useSprites = (texLuke && texAranha && texRede);

    // Carregar fonte (necessária para texto real na transição)
    TTF_Font *font = TTF_OpenFont("assets/DejaVuSans.ttf", 36);
    if (!font) {
        SDL_Log("Aviso: não conseguiu abrir fonte 'assets/DejaVuSans.ttf': %s", TTF_GetError());
        // Ainda prossegue, mas sem texto renderizado (será fallback)
    }

    // Carregar música/som de transição (buscar em assets/transition.mp3 ou .wav)
    Mix_Music *transitionMusic = Mix_LoadMUS("assets/transition.mp3");
    if (!transitionMusic) {
        // tentar WAV como fallback
        transitionMusic = Mix_LoadMUS("assets/transition.wav");
        if (!transitionMusic) {
            SDL_Log("Aviso: não encontrou 'assets/transition.(mp3|wav)': %s", Mix_GetError());
        }
    }

    SDL_Color textColor = {255, 255, 255, 255};
    srand((unsigned)time(NULL));

    // --- Personagem ---
    Character Luke;
    Luke.ret.w = 50;
    Luke.ret.h = 100;
    Luke.ret.x = 100;
    Luke.ret.y = WINDOW_ALT - 150;
    Luke.veloc = 10;
    Luke.velPulo = -15;
    Luke.velY = 0;
    Luke.pulando = false;
    Luke.abaixando = false;
    Luke.coracoes = MAX_CORACOES;
    Luke.vidas = MAX_VIDAS;
    Luke.invencivel = 0;
    Luke.facing = 1;

    // --- Controle de fase ---
    int faseAtual = 1;
    int cameraX = 0;
    int faseLargura = 3000;

    // --- Obstáculos fase 1 ---
    Obstaculo obstaculos[MAX_OBSTACULOS];
    int qtdObs = 0;
    obstaculos[qtdObs++] = (Obstaculo){{0, WINDOW_ALT - 50, faseLargura, 50}, true, false};    //chão
    obstaculos[qtdObs++] = (Obstaculo){{600, WINDOW_ALT - 120, 100, 20}, true, true};
    obstaculos[qtdObs++] = (Obstaculo){{1000, WINDOW_ALT - 200, 150, 20}, true, true};
    obstaculos[qtdObs++] = (Obstaculo){{1300, WINDOW_ALT - 250, 150, 20}, true, true};
    obstaculos[qtdObs++] = (Obstaculo){{2000, WINDOW_ALT - 120, 100, 20}, true, true};

    SDL_Rect porta = {2800, WINDOW_ALT - 150, 50, 100};

    // --- Inimigos ---
    Inimigo inimigos[MAX_INIMIGOS] = {0};
    int qtdInimigos = 3;
    inimigos[0] = (Inimigo){{800, WINDOW_ALT - 150, 50, 50}, -1, 2, 0, true};
    inimigos[1] = (Inimigo){{1600, WINDOW_ALT - 150, 50, 50}, 1, 3, 0, true};
    inimigos[2] = (Inimigo){{2300, WINDOW_ALT - 150, 50, 50}, -1, 2, 0, true};

    Uint32 ultimoSpawn = SDL_GetTicks();

    //Projéteis ("redes")
    Rede redes[MAX_REDES];
    for (int i = 0; i < MAX_REDES; i++) {
        redes[i].ativo = false;
        redes[i].ret = (SDL_Rect){0,0,REDE_W,REDE_H};
        redes[i].velocidade = 0;
    }

    //Contador de inimigos destruídos --
    int contadorInimigosMortos = 0;

    //---Estados de tecla---
    bool esqPress = false, dirPress = false, baixoPress = false, puloPress = false;
    bool rodando = true;
    SDL_Event e;

    // -----------------------------
    // --- TRANSITION (ADICIONADO) -
    // -----------------------------
    bool transitionActive = false;     // indicador de transição
    Uint32 transitionStartMs = 0;      // quando a transição começou
    int transitionDestPhase = 2;       // fase destino (fase 2)

    // Mensagem a ser exibida (usar exatamente o texto pedido)
    const char *transitionMessage = "Você está mundando de fase";

    // --------------------------------------------------
    // --- Loop Principal (mantém fase1 inalterada até trigger)
    // --------------------------------------------------
    while (rodando) {
        // eventos
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                rodando = false;
            }
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_LEFT: esqPress = true; break;
                    case SDL_SCANCODE_RIGHT: dirPress = true; break;
                    case SDL_SCANCODE_DOWN: baixoPress = true; break;
                    case SDL_SCANCODE_SPACE: puloPress = true; break;
                    case SDL_SCANCODE_X: {
    for (int idx = 0; idx < MAX_REDES; idx++) {
        if (!redes[idx].ativo) {
            redes[idx].ret.x = Luke.ret.x + Luke.ret.w / 2 - REDE_W / 2;
            redes[idx].ret.y = Luke.ret.y + Luke.ret.h / 2 - REDE_H / 2;

            if (faseAtual == 1) {
                // Fase 1: dispara horizontalmente
                redes[idx].velocidade = 12 * Luke.facing;
            } else if (faseAtual == 2) {
                // Fase 2: dispara para cima
                redes[idx].velocidade = -12; // velocidade vertical negativa (subindo)
            }

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
                    default: break;
                }
            }
        }

        // --- Se transição ativa: pausar updates do jogo e apenas renderizar overlay ---
        if (transitionActive) {
            Uint32 now = SDL_GetTicks();
            Uint32 elapsed = now - transitionStartMs;
            if (elapsed >= TRANSITION_MS) {
                // fim da transição -> aplica fase destino
                transitionActive = false;
                faseAtual = transitionDestPhase;
                // reset básicos para nova fase (mesma lógica já usada antes)
                Luke.ret.x = 100;
                Luke.ret.y = WINDOW_ALT - 150;
                Luke.velY = 0;
                cameraX = 0;
                for (int i = 0; i < MAX_INIMIGOS; i++) inimigos[i].ativo = false;
                contadorInimigosMortos = 0;
                // parar música de transição se estiver tocando
                if (transitionMusic) Mix_HaltMusic();
            } else {
                // enquanto transição: apenas render (lógica abaixo fará overlay com fade)
                // não realiza atualizações de física/jogo aqui
            }
        } else {
            // --- Movimento horizontal (idêntico ao original) ---
            int movimentoX = (dirPress - esqPress) * Luke.veloc;
            Luke.ret.x += movimentoX;
            if (movimentoX > 0) Luke.facing = 1;
            else if (movimentoX < 0) Luke.facing = -1;

            // --- Colisão lateral ---
            for (int i = 0; i < qtdObs && faseAtual == 1; i++) {
                if (!obstaculos[i].solido) continue;
                if (colidem(Luke.ret, obstaculos[i].ret)) {
                    if (movimentoX > 0) {
                        Luke.ret.x = obstaculos[i].ret.x - Luke.ret.w;
                    } else if (movimentoX < 0) {
                        Luke.ret.x = obstaculos[i].ret.x + obstaculos[i].ret.w;
                    }
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
                        } else if (Luke.velY < 0 && Luke.ret.y < obstaculos[i].ret.y + obstaculos[i].ret.h) {
                            Luke.ret.y = obstaculos[i].ret.y + obstaculos[i].ret.h;
                            Luke.velY = 0;
                            Luke.pulando = true;
                            noChao = false;
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

            if (!noChao && Luke.velY == 0) Luke.pulando = true;

            // --- Abaixar ---
            if (baixoPress && !Luke.pulando) {
                if (!Luke.abaixando) {
                    Luke.abaixando = true;
                    Luke.ret.h = 50;
                    Luke.ret.y += 50;
                }
            } else if (Luke.abaixando && podeLevantar(Luke, obstaculos, qtdObs)) {
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
                    if (inimigos[i].ret.y > WINDOW_ALT) inimigos[i].ativo = false;
                }
            }

            // Atualizar redes (movimentação + desativar quando fora da tela)
for (int r = 0; r < MAX_REDES; r++) {
    if (!redes[r].ativo) continue;

    if (faseAtual == 1) {
        // Fase 1: redes horizontais
        redes[r].ret.x += redes[r].velocidade;
        if (redes[r].ret.x > faseLargura || (redes[r].ret.x + redes[r].ret.w) < 0)
            redes[r].ativo = false;
    } else if (faseAtual == 2) {
        // Fase 2: redes verticais (para cima)
        redes[r].ret.y += redes[r].velocidade;
        if (redes[r].ret.y + redes[r].ret.h < 0)
            redes[r].ativo = false;
    }
}


            // Colisão entre redes e inimigos
            for (int r = 0; r < MAX_REDES; r++) {
                if (!redes[r].ativo) continue;
                for (int i = 0; i < MAX_INIMIGOS; i++) {
                    if (!inimigos[i].ativo) continue;
                    if (colidem(redes[r].ret, inimigos[i].ret)) {
                        inimigos[i].ativo = false;
                        redes[r].ativo = false;
                        contadorInimigosMortos++;
                        if (contadorInimigosMortos % 10 == 0) {
                            if (Luke.vidas < MAX_VIDAS) Luke.vidas++;
                        }
                        break;
                    }
                }
            }

            // --- Dano por colisão (jogador) ---
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

            // --- Detecta trigger de transição (colisão com porta na fase 1) ---
            if (faseAtual == 1) {
                SDL_Rect lukeTela = {Luke.ret.x - cameraX, Luke.ret.y, Luke.ret.w, Luke.ret.h};
                SDL_Rect portaTela = porta;
                portaTela.x -= cameraX;
                if (colidem(lukeTela, portaTela)) {
                    // ativa TRANSIÇÃO (não altera fase ainda)
                    transitionActive = true;
                    transitionStartMs = SDL_GetTicks();
                    transitionDestPhase = 2;
                    // iniciar música de transição (se disponível)
                    if (transitionMusic) {
                        Mix_PlayMusic(transitionMusic, 1); // toca uma vez
                    }
                }
            }

            // Limites do jogador e câmera
            if (Luke.ret.x < 0) Luke.ret.x = 0;
            if (Luke.ret.x + Luke.ret.w > faseLargura) Luke.ret.x = faseLargura - Luke.ret.w;
            cameraX = Luke.ret.x + Luke.ret.w / 2 - WINDOW_LARG / 2;
            if (cameraX < 0) cameraX = 0;
            if (cameraX > faseLargura - WINDOW_LARG) cameraX = faseLargura - WINDOW_LARG;
        } // fim bloco updates (transitionActive ? else)

        // -----------------------
        // Renderização (sempre feita)
        // -----------------------
        SDL_SetRenderDrawColor(renderer, 20, 20, 70, 255);
        SDL_RenderClear(renderer);

        // 🎨 Fundo estático — NÃO se move com a câmera
        if (faseAtual == 3) SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
        else SDL_SetRenderDrawColor(renderer, 80, 120, 200, 255);
        SDL_Rect ceu = {0, 0, WINDOW_LARG, WINDOW_ALT};
        SDL_RenderFillRect(renderer, &ceu);

        // Montanhas (fixas)
        SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
        SDL_Rect montanha1 = {100, WINDOW_ALT - 200, 300, 200};
        SDL_Rect montanha2 = {700, WINDOW_ALT - 250, 400, 250};
        SDL_RenderFillRect(renderer, &montanha1);
        SDL_RenderFillRect(renderer, &montanha2);

        // Sol (fixo)
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
        SDL_Rect sol = {WINDOW_LARG - 150, 100, 80, 80};
        SDL_RenderFillRect(renderer, &sol);

        // Fase 1: obstáculos (com câmera)
        if (faseAtual == 1) {
            for (int i = 0; i < qtdObs; i++) {
                SDL_Rect obsTela = obstaculos[i].ret;
                obsTela.x -= cameraX;
                SDL_SetRenderDrawColor(renderer, 100, 60, 20, 255);
                SDL_RenderFillRect(renderer, &obsTela);
            }
            // desenha porta fase1 (verde)
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
            SDL_Rect portaTela = { porta.x - cameraX, porta.y, porta.w, porta.h };
            SDL_RenderFillRect(renderer, &portaTela);
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

        // Redes
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        for (int r = 0; r < MAX_REDES; r++) {
            if (!redes[r].ativo) continue;
            SDL_Rect redeTela = redes[r].ret;
            if (faseAtual == 1) redeTela.x -= cameraX;
            SDL_RenderFillRect(renderer, &redeTela);
        }

        // Luke
        SDL_Rect lukeTela = {Luke.ret.x - (faseAtual == 1 ? cameraX : 0), Luke.ret.y, Luke.ret.w, Luke.ret.h};
        if (Luke.invencivel % 10 < 5)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        else
            SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
        SDL_RenderFillRect(renderer, &lukeTela);

        // HUD (corações e vidas)
        int margem = 20;
        int coracaoTam = 25;
        for (int i = 0; i < MAX_CORACOES; i++) {
            if (i < Luke.coracoes) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            else SDL_SetRenderDrawColor(renderer, 60, 0, 0, 255);
            SDL_Rect c = {margem + i * (coracaoTam + 5), margem, coracaoTam, coracaoTam};
            SDL_RenderFillRect(renderer, &c);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect vidasRect = {margem, margem + 40, 20 * Luke.vidas, 10};
        SDL_RenderFillRect(renderer, &vidasRect);

        // --- Se transição ativa: desenhar overlay com fade in/out e texto + som (som já iniciado) ---
        if (transitionActive) {
            Uint32 now = SDL_GetTicks();
            Uint32 elapsed = now - transitionStartMs; // 0 .. TRANSITION_MS-1
            // calcular alpha do overlay/text: 0..255
            Uint8 alpha = 255;
            if (elapsed < FADE_IN_MS) {
                // fade in: 0 -> 255
                float t = (float)elapsed / (float)FADE_IN_MS;
                alpha = (Uint8)(t * 255.0f);
            } else if (elapsed < FADE_IN_MS + HOLD_MS) {
                // hold: 255
                alpha = 255;
            } else {
                // fade out
                Uint32 e2 = elapsed - (FADE_IN_MS + HOLD_MS);
                if (e2 >= FADE_OUT_MS) alpha = 0;
                else {
                    float t = 1.0f - ((float)e2 / (float)FADE_OUT_MS);
                    alpha = (Uint8)(t * 255.0f);
                }
            }

            // desenhar overlay semi-transparente (usando alpha)
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha / 2); // meio-transparente
            SDL_Rect overlay = {0, 0, WINDOW_LARG, WINDOW_ALT};
            SDL_RenderFillRect(renderer, &overlay);

            // caixa de texto central com opacidade proporcional (usar alpha)
            int boxW = 560, boxH = 120;
            SDL_Rect box = { WINDOW_LARG/2 - boxW/2, WINDOW_ALT/2 - boxH/2, boxW, boxH };
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, alpha);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &box);

            // borda preta opaca
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
            SDL_RenderDrawRect(renderer, &box);

            // Texto centralizado (usar TTF quando disponível)
            if (font) {
                SDL_Surface *surf = TTF_RenderUTF8_Blended(font, transitionMessage, textColor);
                if (surf) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
                    int tw = surf->w, th = surf->h;
                    SDL_FreeSurface(surf);
                    if (tex) {
                        // ajustar alpha do texture se possível
                        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                        SDL_SetTextureAlphaMod(tex, alpha);
                        SDL_Rect dst = { WINDOW_LARG/2 - tw/2, WINDOW_ALT/2 - th/2, tw, th };
                        SDL_RenderCopy(renderer, tex, NULL, &dst);
                        SDL_DestroyTexture(tex);
                    }
                }
            } else {
                // fallback: desenhar texto "rudimentar" com retângulos (se fonte não carregou)
                SDL_SetRenderDrawColor(renderer, 0,0,0,alpha);
                SDL_Rect small = { WINDOW_LARG/2 - 200, WINDOW_ALT/2 - 12, 400, 24 };
                SDL_RenderFillRect(renderer, &small);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    // limpagem
    if (transitionMusic) Mix_FreeMusic(transitionMusic);
    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;

cleanup_window:
    SDL_DestroyWindow(window);
cleanup_sdl:
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 1;
}

