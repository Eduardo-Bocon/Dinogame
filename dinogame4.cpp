#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Definiçao de constantes
#define MAX_DINOS 12
#define MAX_MISSILES 5
#define SCREEN_WIDTH 60
#define SCREEN_HEIGHT 25
#define COLLISION_DISTANCE 2
#define TRUCK_ARRIVAL_TIME 8000 // Tempo em milissegundos para o caminhao reabastecer
#define TRUCK_SPEED 1            // Velocidade do caminhao (quanto menor, mais rapido)

// Variaveis do jogo
int depot_slots;      // Numero de slots de misseis no deposito
int depot_missiles;   // Numero de misseis no deposito
int missiles_needed;  // Numero de misseis necessarios para matar um dinossauro
int current_dinos;    // Numero de dinossauros atualmente vivos
int total_dinos;      // Total de dinossauros gerados
bool game_over = false;
int score = 0; // Pontuaçao do jogador


// Estrutura do helicoptero
typedef struct {
    int x;
    int y;
    int missiles;
    bool destroyed;
    bool reloading;
    int direction; // 1 para esquerda para direita, -1 para direita para esquerda
} Helicopter;

Helicopter helicopter;

// Estrutura do dinossauro
typedef struct {
    int x;
    int y;
    int health;
    bool alive;
    int direction; // 1 para esquerda para direita, -1 para direita para esquerda
} Dino;

Dino dinos[MAX_DINOS];

// Estrutura dos misseis
typedef struct {
    int x, y;
    bool active;
    pthread_t thread_id; // Thread ID para cada missil
} Missile;

Missile missiles[MAX_MISSILES]; // Array de misseis

// Variaveis do caminhao
typedef struct {
    int x;
    int y;
    bool active; // Se o caminhao esta visivel/movendo
} Truck;

Truck truck = {SCREEN_WIDTH, SCREEN_HEIGHT - 2, false};

// Mutexes e variaveis de condiçao
pthread_mutex_t depot_mutex;
pthread_mutex_t helicopter_mutex;
pthread_mutex_t dinos_mutex;
pthread_mutex_t console_mutex;

// Declaraçao das threads
pthread_t input_thread;
pthread_t truck_thread;
pthread_t dino_manager_thread;
pthread_t dino_threads[MAX_DINOS];

// Declaraçao das funçoes
void *input_function(void *arg);
void *truck_function(void *arg);
void *dino_function(void *arg);
void *dino_manager_function(void *arg);
void *missile_function(void *arg); // Funçao para cada missil
void init_game();
void game_loop();
void render();
void check_collision();
void check_restock();
void draw_helicopter();
void draw_dino(int index);
void draw_depot();
void draw_truck();
void clear_screen();
void gotoxy(int x, int y);
void hidecursor();
void display_congratulations();

int main() {
    init_game(); // Inicializa o jogo
    game_loop(); // Inicia o loop do jogo
    return 0;
}

void init_game() {
    // Inicializa as variaveis do jogo e define o nivel de dificuldade
    int difficulty;
    printf("Selecione o nivel de dificuldade (1-Facil, 2-Medio, 3-Dificil): ");
    scanf("%d", &difficulty);
    switch (difficulty) {
        case 1:
            missiles_needed = 3;
            depot_slots = 10;
            break;
        case 2:
            missiles_needed = 5;
            depot_slots = 7;
            break;
        case 3:
            missiles_needed = 7;
            depot_slots = 5;
            break;
        default:
            missiles_needed = 5;
            depot_slots = 7;
            break;
    }

    depot_missiles = depot_slots;
    helicopter.missiles = depot_slots;
    helicopter.destroyed = false;
    helicopter.reloading = false;
    helicopter.x = 10;
    helicopter.y = 5;
    helicopter.direction = 1; // Esquerda para direita
    current_dinos = 1;
    total_dinos = 1;
    game_over = false;

    // Inicializa o primeiro dinossauro
    dinos[0].x = SCREEN_WIDTH - 10;
    dinos[0].y = SCREEN_HEIGHT - 10;
    dinos[0].health = missiles_needed;
    dinos[0].alive = true;
    dinos[0].direction = -1; // Direita para esquerda

    // Inicializa os misseis
    for (int i = 0; i < MAX_MISSILES; i++) {
        missiles[i].active = false;
    }

    // Inicializa mutexes
    pthread_mutex_init(&depot_mutex, NULL);
    pthread_mutex_init(&helicopter_mutex, NULL);
    pthread_mutex_init(&dinos_mutex, NULL);
    pthread_mutex_init(&console_mutex, NULL);

    // Cria threads
    pthread_create(&input_thread, NULL, input_function, NULL);
    pthread_create(&truck_thread, NULL, truck_function, NULL);
    pthread_create(&dino_manager_thread, NULL, dino_manager_function, NULL);
    pthread_create(&dino_threads[0], NULL, dino_function, (void *)(intptr_t)0);

    // Limpa e renderiza a tela inicial
    clear_screen();
    render();

    // Esconde o cursor
    hidecursor();
}

void game_loop() {
    while (!game_over && !helicopter.destroyed) {
        render(); // Atualiza a tela
        check_collision(); // Verifica colisoes
        check_restock(); // Verifica reabastecimento do helicoptero
        Sleep(25); // Pausa para controle de tempo
    }

    printf("\nFim de jogo! Pressione qualquer tecla para sair.\n");
}

void render() {
    pthread_mutex_lock(&console_mutex);

    // Limpa a tela
    clear_screen();

    // Desenha o helicoptero
    pthread_mutex_lock(&helicopter_mutex);
    draw_helicopter();
    pthread_mutex_unlock(&helicopter_mutex);

    // Desenha os dinossauros
    pthread_mutex_lock(&dinos_mutex);
    for (int i = 0; i < total_dinos; i++) {
        if (dinos[i].alive) {
            draw_dino(i);
        }
    }
    pthread_mutex_unlock(&dinos_mutex);

    // Desenha os misseis
    for (int i = 0; i < MAX_MISSILES; i++) {
        if (missiles[i].active) {
            gotoxy(missiles[i].x, missiles[i].y);
            printf("-");
        }
    }

    // Desenha o deposito
    draw_depot();

    // Desenha o caminhao se estiver ativo
    if (truck.active) {
        draw_truck();
    }

    // Exibe informaçoes do jogo
    gotoxy(0, 0);
    printf("Misseis no helicoptero: %d   ", helicopter.missiles);
    printf("Misseis no deposito: %d   ", depot_missiles);
    printf("Dinossauros atuais: %d   ", current_dinos);
    gotoxy(0, 1); // Exibe o score na segunda linha
    printf("Pontuacao: %d", score);

    pthread_mutex_unlock(&console_mutex);
}

void *input_function(void *arg) {
    while (!game_over && !helicopter.destroyed) {
        if (kbhit()) { // Detecta tecla pressionada
            int ch = getch();
            if (ch == 0 || ch == 224) { // Teclas especiais
                ch = getch();
                pthread_mutex_lock(&helicopter_mutex);
                switch (ch) {
                    case 72: // Seta para cima
                        helicopter.y = (helicopter.y > 0) ? helicopter.y - 1 : 0;
                        break;
                    case 80: // Seta para baixo
                        helicopter.y = (helicopter.y < SCREEN_HEIGHT - 3) ? helicopter.y + 1 : SCREEN_HEIGHT - 3;
                        break;
                    case 75: // Seta para esquerda
                        helicopter.x = (helicopter.x > 0) ? helicopter.x - 1 : 0;
                        helicopter.direction = -1;
                        break;
                    case 77: // Seta para direita
                        helicopter.x = (helicopter.x < SCREEN_WIDTH - 13) ? helicopter.x + 1 : SCREEN_WIDTH - 13;
                        helicopter.direction = 1;
                        break;
                }
                pthread_mutex_unlock(&helicopter_mutex);
            } else if (ch == ' ') { // Barra de espaço para disparar
                pthread_mutex_lock(&helicopter_mutex);
                if (helicopter.missiles > 0) {
                    for (int i = 0; i < MAX_MISSILES; i++) {
                        if (!missiles[i].active) {
                            missiles[i].x = helicopter.x + 3;
                            missiles[i].y = helicopter.y;
                            missiles[i].active = true;
                            helicopter.missiles--;
                            // Cria uma thread para o missil
                            pthread_create(&missiles[i].thread_id, NULL, missile_function, (void *)(intptr_t)i);
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&helicopter_mutex);
            }
        }
        Sleep(10); // Reduz o uso da CPU
    }
    return NULL;
}

void *missile_function(void *arg) {
    int index = (int)(intptr_t)arg;
    while (missiles[index].active && !game_over) {
        pthread_mutex_lock(&console_mutex);
        missiles[index].x += 4; // Aumenta a velocidade dos misseis

        if (missiles[index].x >= SCREEN_WIDTH) {
            missiles[index].active = false; // Desativa o missil ao sair da tela
            pthread_mutex_unlock(&console_mutex);
            break; // Sai do loop e termina a thread
        }

        // Verifica colisao com dinossauros
        pthread_mutex_lock(&dinos_mutex);
        for (int j = 0; j < total_dinos; j++) {
            if (dinos[j].alive && abs(missiles[index].x - dinos[j].x) < COLLISION_DISTANCE &&
                abs(missiles[index].y - dinos[j].y) < COLLISION_DISTANCE) {
                dinos[j].health--;
                missiles[index].active = false; // Desativa o missil
                if (dinos[j].health <= 0) {
                    dinos[j].alive = false;
                    current_dinos--;
                    score += 10; // Incrementa a pontuaçao ao eliminar um dinossauro
                }
                break; // Um missil so pode atingir um dinossauro
            }
        }
        pthread_mutex_unlock(&dinos_mutex);
        pthread_mutex_unlock(&console_mutex);

        Sleep(50); // Controla a velocidade do missil
    }
    return NULL;
}

void check_collision() {
    pthread_mutex_lock(&helicopter_mutex);
    pthread_mutex_lock(&dinos_mutex);
    for (int i = 0; i < total_dinos; i++) {
        if (dinos[i].alive && abs(helicopter.x - dinos[i].x) < COLLISION_DISTANCE &&
            abs(helicopter.y - dinos[i].y) < COLLISION_DISTANCE) {
            helicopter.destroyed = true;
            game_over = true;
            printf("\nHelicoptero destruido por um dinossauro!\n");
        }
    }
    pthread_mutex_unlock(&dinos_mutex);
    pthread_mutex_unlock(&helicopter_mutex);
}

void check_restock() {
    pthread_mutex_lock(&helicopter_mutex);
    pthread_mutex_lock(&depot_mutex);

    // Define o centro e o alcance do deposito
    int depot_x = 5;
    int depot_y = SCREEN_HEIGHT - 5;
    int depot_range = 3; // Aumente este valor para ampliar o alcance

    // Verifica se o helicoptero esta dentro do alcance do deposito
    if (abs(helicopter.x - depot_x) <= depot_range && abs(helicopter.y - depot_y) <= depot_range) {
        int restock = depot_slots - helicopter.missiles; // Quantidade que o helicoptero pode recarregar
        if (restock > 0 && depot_missiles > 0) {
            int reload_amount = (depot_missiles >= restock) ? restock : depot_missiles;
            helicopter.missiles += reload_amount;
            depot_missiles -= reload_amount;
            printf("\nHelicoptero reabastecido com %d misseis!\n", reload_amount);
        }
    }

    pthread_mutex_unlock(&depot_mutex);
    pthread_mutex_unlock(&helicopter_mutex);
}

void *dino_function(void *arg) {
    int index = (int)(intptr_t)arg;
    while (dinos[index].alive && !game_over) {
        pthread_mutex_lock(&dinos_mutex);
        dinos[index].x += dinos[index].direction;

        // Altera direçao ao atingir as bordas
        if (dinos[index].x <= 0 || dinos[index].x >= SCREEN_WIDTH - 10) {
            dinos[index].direction *= -1;
        }
        pthread_mutex_unlock(&dinos_mutex);

        Sleep(200); // Velocidade de movimento dos dinossauros
    }
    return NULL;
}

void *dino_manager_function(void *arg) {
    while (!game_over) {
        Sleep(7000); // Gera novos dinossauros a cada 7 segundos
        pthread_mutex_lock(&dinos_mutex);
        if (total_dinos < MAX_DINOS) {
            int index = total_dinos;
            dinos[index].x = SCREEN_WIDTH - 15;
            dinos[index].y = rand() % (SCREEN_HEIGHT - 15);
            dinos[index].health = missiles_needed;
            dinos[index].alive = true;
            dinos[index].direction = rand() % 2 == 0 ? -1 : 1;
            total_dinos++;
            current_dinos++;
            pthread_create(&dino_threads[index], NULL, dino_function, (void *)(intptr_t)index);
        }
        if (current_dinos >= 5) {
            game_over = true;
            printf("\nGame Over! Muitos dinossauros na tela!\n");
        }
        if (total_dinos >= MAX_DINOS && current_dinos == 0) {
            game_over = true;
            pthread_mutex_unlock(&dinos_mutex);
            display_congratulations(); // Exibe mensagem de "Parabens!!!"
            break;
        }
        pthread_mutex_unlock(&dinos_mutex);
    }
    return NULL;
}

void *truck_function(void *arg) {
    while (!game_over) {
        Sleep(TRUCK_ARRIVAL_TIME); // Espera ate o caminhao aparecer
        pthread_mutex_lock(&depot_mutex);
        truck.active = true;
        truck.x = SCREEN_WIDTH; // Caminhao começa na direita da tela
        pthread_mutex_unlock(&depot_mutex);

        // Move o caminhao para a esquerda
        while (truck.x > 5 && !game_over) {
            pthread_mutex_lock(&console_mutex);
            truck.x -= TRUCK_SPEED;
            pthread_mutex_unlock(&console_mutex);
            Sleep(100); // Controle de velocidade do caminhao
        }

        // Quando o caminhao chega ao deposito
        pthread_mutex_lock(&depot_mutex);
        depot_missiles = depot_slots; // Reabastece o deposito
		truck.active = false;        // Caminhao desaparece apos o reabastecimento
        //Sleep(1000);
		//printf("\nCaminhao abasteceu o deposito!\n");
        pthread_mutex_unlock(&depot_mutex);
        
         pthread_mutex_lock(&console_mutex);
        truck.active = true;
        pthread_mutex_unlock(&console_mutex);
        
        // Move o caminhao para a direita
        while (truck.x < SCREEN_WIDTH && !game_over) {
            pthread_mutex_lock(&console_mutex);
            truck.x += TRUCK_SPEED;
            pthread_mutex_unlock(&console_mutex);
            Sleep(75); // Controle de velocidade do caminhao
   		 }
    	pthread_mutex_lock(&console_mutex);
        truck.active = false  ;
        pthread_mutex_unlock(&console_mutex);
	}
    return NULL;
}

void draw_truck() {
    gotoxy(truck.x, truck.y);
    printf(" ::::-...  ");
    gotoxy(truck.x, truck.y + 1);
    printf("  =+=+==-  ");
}

void draw_helicopter() {
    gotoxy(helicopter.x, helicopter.y);
    printf("--++-- ");
    gotoxy(helicopter.x, helicopter.y + 1);
    printf("..###..");
    gotoxy(helicopter.x, helicopter.y + 2);
    printf("*     *");
}

void draw_depot() {
    int x = 5, y = SCREEN_HEIGHT - 5;
    gotoxy(x, y);
    printf("  _____  ");
    gotoxy(x, y + 1);
    printf(" /     \\ ");
    gotoxy(x, y + 2);
    printf("|       |");
    gotoxy(x, y + 3);
    printf("|       |");
    gotoxy(x, y + 4);
    printf("|_______|");
}

void draw_dino(int index) {
    int x = dinos[index].x;
    int y = dinos[index].y;
    gotoxy(x, y);
    printf("  .---:     ");
    gotoxy(x, y + 1);
    printf("  :++**.    ");
    gotoxy(x, y + 2);
    printf("  .-**=:  : ");
    gotoxy(x, y + 3);
    printf("    =*****: ");
    gotoxy(x, y + 4);
    printf("     =+*=.  ");
}

void display_congratulations() {
    clear_screen();
    int center_x = SCREEN_WIDTH / 2 - 5; // Ajusta para centralizar "Parabens!!!"
    int center_y = SCREEN_HEIGHT / 2;

    // Exibe a mensagem no centro da tela
    gotoxy(center_x, center_y);
    printf("Parabens!!!");
    gotoxy(center_x - 10, center_y + 2);
    printf("Voce derrotou todos os dinossauros!");

    // Exibe o score final
    gotoxy(center_x - 8, center_y + 4);
    printf("Pontuaçao final: %d", score);

    // Mensagem de pausa
    gotoxy(center_x - 15, center_y + 6);
    printf("Aguarde...");

    // Pausa por 5 segundos
    Sleep(5000); // Tempo em milissegundos (5 segundos)

    // Permite que o jogador saia
    gotoxy(center_x - 15, center_y + 6);
    printf("Pressione qualquer tecla para sair...");
    getch(); // Espera o usuario pressionar uma tecla
}

void clear_screen() {
    system("cls");
}

void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void hidecursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 1;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}
