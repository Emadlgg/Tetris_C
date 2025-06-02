#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>  
#include <windows.h> 
#include <conio.h>   

#define WIDTH 10
#define HEIGHT 20

// Estructura para las piezas de Tetris
typedef struct {
    int shape[4][4];
    int x, y;  // Posición de la pieza
} Piece;

// Estructura para controlar el juego
typedef struct {
    int board[HEIGHT][WIDTH];
    int score;
} GameControl;

GameControl gameControl;
Piece currentPiece;
pthread_mutex_t gameOverMutex;
pthread_cond_t piecePlacedCondition;
sem_t boardSemaphore;
bool gameOver = false;

// Definir las piezas de Tetris
const Piece pieces[] = {
    { { {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} }, 0, 0 }, // I
    { { {1, 1, 1}, {0, 1, 0}, {0, 0, 0} }, 0, 0 }, // T
    { { {1, 1}, {1, 1}, {0, 0} }, 0, 0 }, // O
    { { {0, 1, 1}, {1, 1, 0}, {0, 0, 0} }, 0, 0 }, // S
    { { {1, 1, 0}, {0, 1, 1}, {0, 0, 0} }, 0, 0 }, // Z
    { { {1, 1, 1}, {1, 0, 0}, {0, 0, 0} }, 0, 0 }, // L
    { { {1, 1, 1}, {0, 0, 1}, {0, 0, 0} }, 0, 0 }, // J
};

// Generar una nueva pieza aleatoria
Piece generateRandomPiece() {
    Piece piece = pieces[rand() % (sizeof(pieces) / sizeof(pieces[0]))];
    piece.x = WIDTH / 2 - 2; // Inicializar en el centro
    piece.y = 0; // Inicializar en la parte superior
    return piece;
}

// Verificar si la pieza puede moverse hacia abajo
bool canMoveDown(Piece* piece) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                int newY = piece->y + i + 1;
                int newX = piece->x + j;
                if (newY >= HEIGHT || gameControl.board[newY][newX]) {
                    return false; // No puede moverse
                }
            }
        }
    }
    return true;
}

// Verificar si la pieza puede moverse a la izquierda
bool canMoveLeft(Piece* piece) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                int newY = piece->y + i;
                int newX = piece->x + j - 1;
                if (newX < 0 || gameControl.board[newY][newX]) {
                    return false; // No puede moverse
                }
            }
        }
    }
    return true;
}

// Verificar si la pieza puede moverse a la derecha
bool canMoveRight(Piece* piece) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                int newY = piece->y + i;
                int newX = piece->x + j + 1;
                if (newX >= WIDTH || gameControl.board[newY][newX]) {
                    return false; // No puede moverse
                }
            }
        }
    }
    return true;
}

// Colocar la pieza en el tablero
void placePiece(Piece* piece, bool place) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                int newY = piece->y + i;
                int newX = piece->x + j;
                if (place) {
                    gameControl.board[newY][newX] = 1; // Fijar la pieza
                } else {
                    gameControl.board[newY][newX] = 0; // Limpiar la pieza
                }
            }
        }
    }
}

// Imprimir el tablero en la consola
void printBoard() {
    system("cls");  // Limpiar la consola
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            bool isPiece = false;
            for (int pi = 0; pi < 4; pi++) {
                for (int pj = 0; pj < 4; pj++) {
                    if (currentPiece.shape[pi][pj] && (currentPiece.y + pi == i) && (currentPiece.x + pj == j)) {
                        isPiece = true;
                        break;
                    }
                }
                if (isPiece) break;
            }
            if (isPiece || gameControl.board[i][j]) {
                printf("[]");  // Píxel lleno
            } else {
                printf("  ");  // Espacio vacío
            }
        }
        printf("\n");
    }
    printf("Puntuación: %d\n", gameControl.score);
}

// Eliminar líneas completas
void removeCompleteLines() {
    for (int i = 0; i < HEIGHT; i++) {
        bool complete = true;
        for (int j = 0; j < WIDTH; j++) {
            if (gameControl.board[i][j] == 0) {
                complete = false;
                break;
            }
        }
        if (complete) {
            // Desplazar líneas hacia abajo
            for (int k = i; k > 0; k--) {
                for (int j = 0; j < WIDTH; j++) {
                    gameControl.board[k][j] = gameControl.board[k - 1][j];
                }
            }
            // Limpiar la línea superior
            for (int j = 0; j < WIDTH; j++) {
                gameControl.board[0][j] = 0;
            }
            gameControl.score += 100; // Incrementar la puntuación
        }
    }
}

// Verificar si se puede generar la nueva pieza
bool canSpawnPiece(Piece* piece) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                if (gameControl.board[piece->y + i][piece->x + j]) {
                    return false; // No se puede generar
                }
            }
        }
    }
    return true;
}

// Función que manejará el hilo para controlar la pieza
void* pieceDropper(void* arg) {
    while (true) {
        pthread_mutex_lock(&gameOverMutex);
        if (gameOver) {
            pthread_mutex_unlock(&gameOverMutex);
            break;
        }
        pthread_mutex_unlock(&gameOverMutex);

        sem_wait(&boardSemaphore); // Bloquear acceso al tablero

        // Limpiar la posición anterior de la pieza
        placePiece(&currentPiece, false);
        printBoard(); // Imprimir el estado antes de mover la pieza

        if (canMoveDown(&currentPiece)) {
            currentPiece.y++; // Mover hacia abajo
        } else {
            placePiece(&currentPiece, true); // Fijar pieza en el tablero
            pthread_cond_signal(&piecePlacedCondition); // Señalar que se ha colocado una pieza
            removeCompleteLines(); // Eliminar líneas completas

            currentPiece = generateRandomPiece(); // Generar nueva pieza

            // Ver si hay Game Over
            pthread_mutex_lock(&gameOverMutex);
            if (!canSpawnPiece(&currentPiece)) {
                gameOver = true;
            }
            pthread_mutex_unlock(&gameOverMutex);
        }

        sem_post(&boardSemaphore); // Liberar acceso al tablero
        printBoard(); // Imprimir el estado después de mover la pieza
        Sleep(500); // Controlar la velocidad de caída de las piezas
    }
    return NULL;
}

// Función para manejar el input del usuario
void* userInput(void* arg) {
    while (!gameOver) {
        char input = getch();
        sem_wait(&boardSemaphore); // Bloquear acceso al tablero

        // Limpiar la posición anterior de la pieza
        placePiece(&currentPiece, false);

        if (input == 'a' && canMoveLeft(&currentPiece)) {
            currentPiece.x--; // Mover izquierda
        } else if (input == 'd' && canMoveRight(&currentPiece)) {
            currentPiece.x++; // Mover derecha
        } else if (input == 's') {
            while (canMoveDown(&currentPiece)) {
                currentPiece.y++; // Mover hacia abajo rápidamente
            }
        } else if (input == 'q') {
            gameOver = true; // Salir del juego
        }

        placePiece(&currentPiece, true); // Fijar pieza en la nueva posición
        sem_post(&boardSemaphore); // Liberar acceso al tablero
        printBoard(); // Imprimir el estado
    }
    return NULL;
}

// Función para controlar el juego de la computadora
void* computerPlayer(void* arg) {
    while (!gameOver) {
        sem_wait(&boardSemaphore); // Bloquear acceso al tablero

        // Limpiar la posición anterior de la pieza
        placePiece(&currentPiece, false);
        
        if (canMoveDown(&currentPiece)) {
            currentPiece.y++; // Mover hacia abajo
        } else {
            placePiece(&currentPiece, true); // Fijar pieza en el tablero
            pthread_cond_signal(&piecePlacedCondition); // Señalar que se ha colocado una pieza
            removeCompleteLines(); // Eliminar líneas completas

            currentPiece = generateRandomPiece(); // Generar nueva pieza

            // Ver si hay Game Over
            if (!canSpawnPiece(&currentPiece)) {
                gameOver = true;
            }
        }

        placePiece(&currentPiece, true); // Fijar pieza en la nueva posición
        sem_post(&boardSemaphore); // Liberar acceso al tablero
        printBoard(); // Imprimir el estado
        Sleep(500); // Controlar la velocidad de caída de las piezas
    }
    return NULL;
}

// Función del menú
void showMenu() {
    int option;
    printf("Bienvenido a Tetris!\n");
    printf("1. Jugar como jugador\n");
    printf("2. Jugar contra computadora\n");
    printf("Por favor, elige una opción: ");
    scanf("%d", &option);
    
    // Iniciar el juego según la opción elegida
    if (option == 1) {
        pthread_t userThread, dropThread;
        pthread_create(&dropThread, NULL, pieceDropper, NULL);
        pthread_create(&userThread, NULL, userInput, NULL);
        pthread_join(userThread, NULL);
        pthread_cancel(dropThread);
    } else if (option == 2) {
        pthread_t computerThread, dropThread;
        pthread_create(&dropThread, NULL, pieceDropper, NULL);
        pthread_create(&computerThread, NULL, computerPlayer, NULL);
        pthread_join(computerThread, NULL);
        pthread_cancel(dropThread);
    } else {
        printf("Opción no válida. Saliendo del juego.\n");
        exit(0);
    }
}

int main() {
    srand(time(NULL)); // Inicializar 
    pthread_mutex_init(&gameOverMutex, NULL);
    sem_init(&boardSemaphore, 0, 1);
    currentPiece = generateRandomPiece(); // Generar primera pieza
    showMenu(); // Mostrar menú
    return 0;
}