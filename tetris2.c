#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <conio.h> // Para _kbhit() y _getch()
#include <windows.h> // Para Sleep()

#define WIDTH 20 // Ancho del tablero
#define HEIGHT 20 // Alto del tablero

// Estructura para representar las piezas
typedef struct {
    char shape[4][4]; // Forma de la pieza
    int width;        // Ancho de la pieza
    int height;       // Alto de la pieza
    int x, y;         // Posición actual de la pieza
} Piece;

// Estructura para manejar el control del juego
typedef struct {
    int score;          // Puntuación del jugador
    bool isPlayerMode;  // Modo de jugador (true para jugador, false para computadora)
} GameControl;

// Variables globales
char board[HEIGHT][WIDTH + 1];
Piece currentPiece;
GameControl gameControl;
bool gameOver = false;
pthread_mutex_t boardMutex = PTHREAD_MUTEX_INITIALIZER;

// Funciones para la lógica del juego
void initializeBoard() {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            board[i][j] = '.';  // Línea vacía
        }
        board[i][WIDTH] = '\0';  // Fin de cadena
    }
}

Piece generateRandomPiece() {
    Piece piece;
    // Formas de las piezas
    char shapes[7][4][4] = {
        {
            {'#', '#', '#', '#'},
            {' ', ' ', ' ', ' '},
            {' ', ' ', ' ', ' '},
            {' ', ' ', ' ', ' '}
        },
        {
            {'#', '#', '#'},
            {' ', '#', ' '},
            {' ', ' ', ' '}
        },
        {
            {'#', '#', '#'},
            {'#', ' ', ' '},
            {' ', ' ', ' '}
        },
        {
            {' ', '#', '#'},
            {'#', '#', ' '},
            {' ', ' ', ' '}
        },
        {
            {'#', '#'},
            {'#', '#' }
        },
        {
            {'#', '#', ' '},
            {' ', '#', ' '},
            {' ', ' ', ' '}
        },
        {
            {' ', '#', '#'},
            {' ', ' ', '#'},
            {' ', ' ', ' '}
        }
    };
    
    int randomIndex = rand() % 7;  // Seleccionar una forma aleatoria
    memcpy(piece.shape, shapes[randomIndex], sizeof(piece.shape));
    piece.width = strlen(piece.shape[0]);  // Ancho de la pieza
    piece.height = sizeof(piece.shape) / sizeof(piece.shape[0]);  // Alto de la pieza
    piece.x = WIDTH / 2 - piece.width / 2;  // Posición inicial X
    piece.y = 0;  // Posición inicial Y
    return piece;
}

void printBoard() {
    system("cls");  // Limpiar consola (en Windows)
    printf("Puntuación: %d\n\n", gameControl.score);
    for (int i = 0; i < HEIGHT; i++) {
        printf("|"); // Bordes del tablero
        printf("%s", board[i]);
        printf("|\n"); // Bordes del tablero
    }
    for (int j = 0; j < WIDTH + 2; j++) { // Línea inferior del tablero
        printf("-");
    }
    printf("\n");
}

bool canMoveDown(Piece* piece) {
    for (int i = 0; i < piece->height; i++) {
        for (int j = 0; j < piece->width; j++) {
            if (piece->shape[i][j] != ' ' && 
                (piece->y + i + 1 >= HEIGHT || 
                board[piece->y + i + 1][piece->x + j] != '.')) {
                return false;
            }
        }
    }
    return true;
}

bool canMoveLeft(Piece* piece) {
    for (int i = 0; i < piece->height; i++) {
        for (int j = 0; j < piece->width; j++) {
            if (piece->shape[i][j] != ' ' && 
                (piece->x + j - 1 < 0 || 
                board[piece->y + i][piece->x + j - 1] != '.')) {
                return false;
            }
        }
    }
    return true;
}

bool canMoveRight(Piece* piece) {
    for (int i = 0; i < piece->height; i++) {
        for (int j = 0; j < piece->width; j++) {
            if (piece->shape[i][j] != ' ' && 
                (piece->x + j + 1 >= WIDTH || 
                board[piece->y + i][piece->x + j + 1] != '.')) {
                return false;
            }
        }
    }
    return true;
}

void rotatePiece(Piece* piece) {
    char temp[4][4] = {0};
    for (int i = 0; i < piece->height; i++) {
        for (int j = 0; j < piece->width; j++) {
            temp[j][piece->height - 1 - i] = piece->shape[i][j];
        }
    }
    memcpy(piece->shape, temp, sizeof(temp));
    int tempWidth = piece->width;
    piece->width = piece->height;
    piece->height = tempWidth;
}

void placePiece(Piece* piece, bool place) {
    for (int i = 0; i < piece->height; i++) {
        for (int j = 0; j < piece->width; j++) {
            if (piece->shape[i][j] != ' ') {
                board[piece->y + i][piece->x + j] = place ? piece->shape[i][j] : '.';
            }
        }
    }
}

void removeCompleteLines() {
    int linesRemoved = 0;
    for (int i = HEIGHT - 1; i >= 0; i--) {
        bool fullLine = true;
        for (int j = 0; j < WIDTH; j++) {
            if (board[i][j] == '.') {
                fullLine = false;
                break;
            }
        }
        if (fullLine) {
            linesRemoved++;
            for (int k = i; k > 0; k--) {
                memcpy(board[k], board[k - 1], WIDTH + 1); // Mover líneas hacia abajo
            }
            memset(board[0], '.', WIDTH); // Nueva línea vacía
            i++; // Volver a verificar la misma línea
        }
    }
    gameControl.score += linesRemoved;  // Incrementar puntuación
}

bool canSpawnPiece(Piece* piece) {
    for (int i = 0; i < piece->height; i++) {
        for (int j = 0; j < piece->width; j++) {
            if (piece->shape[i][j] != ' ' && board[piece->y + i][piece->x + j] != '.') {
                return false;
            }
        }
    }
    return true;
}

// Hilo para manejar el descenso automático de las piezas en modo jugador
void* pieceDropper(void* arg) {
    while (!gameOver) {
        currentPiece = generateRandomPiece();

        if (!canSpawnPiece(&currentPiece)) {
            gameOver = true;  // Si no se puede generar, se marca Game Over
            return NULL;
        }

        placePiece(&currentPiece, true);  // Colocar la pieza en la posición inicial

        while (!gameOver) {
            placePiece(&currentPiece, false);  // Limpiar la posición anterior

            if (canMoveDown(&currentPiece)) {
                currentPiece.y++;  // Mover la pieza hacia abajo
            } else {
                placePiece(&currentPiece, true);  // Fijar la pieza en el tablero
                removeCompleteLines(); // Lógica para eliminar líneas completas
                break;  // Salir del bucle
            }

            placePiece(&currentPiece, true);  // Colocar la pieza en la nueva posición
            printBoard();  // Imprimir el estado del tablero
            Sleep(500);  // Controlar la velocidad de caída
        }
    }

    return NULL;
}

// Hilo para manejar entradas del jugador
int kbhit(void) {
    return _kbhit(); // Llama a la función _kbhit()
}

void* inputHandler(void* arg) {
    while (!gameOver) {
        if (kbhit()) {
            char key = _getch(); // Captura la tecla presionada
            if (key == 'q') {
                gameOver = true; // Salir del juego
            } else {
                placePiece(&currentPiece, false);  // Limpiar la posición anterior
                if (key == 'a' && canMoveLeft(&currentPiece)) {
                    currentPiece.x--;  // Mover a la izquierda
                } else if (key == 'd' && canMoveRight(&currentPiece)) {
                    currentPiece.x++;  // Mover a la derecha
                } else if (key == 'w') {
                    rotatePiece(&currentPiece);  // Rotar la pieza
                } else if (key == 's') {
                    // Baja la pieza un poco más rápido, pero no al fondo
                    if (canMoveDown(&currentPiece)) {
                        currentPiece.y++;
                    }
                }
                placePiece(&currentPiece, true);  // Colocar la pieza en la nueva posición
                printBoard();  // Imprimir el estado del tablero
            }
        }
        Sleep(50);  // Evitar que el bucle consuma demasiados recursos
    }
    return NULL;
}

// Hilo para el modo computadora
void* computerMode(void* arg) {
    while (!gameOver) {
        currentPiece = generateRandomPiece();

        if (!canSpawnPiece(&currentPiece)) {
            gameOver = true;  // Si no se puede generar, se marca Game Over
            return NULL;
        }

        placePiece(&currentPiece, true);  // Colocar la pieza en la posición inicial

        while (!gameOver) {
            placePiece(&currentPiece, false);  // Limpiar la posición anterior

            if (canMoveDown(&currentPiece)) {
                currentPiece.y++;  // Mover la pieza hacia abajo
            } else {
                placePiece(&currentPiece, true);  // Fijar la pieza en el tablero
                removeCompleteLines(); // Lógica para eliminar líneas completas
                break;  // Salir del bucle
            }

            placePiece(&currentPiece, true);  // Colocar la pieza en la nueva posición
            printBoard();  // Imprimir el estado del tablero
            Sleep(500);  // Controlar la velocidad de caída
        }
    }

    return NULL;
}

// Función para mostrar el menú
void showMenu() {
    printf("=== MENÚ DE JUEGO ===\n");
    printf("1. Modo Jugador\n");
    printf("2. Modo Computadora\n");
    printf("Q. Salir\n");
    printf("=====================\n");
}

int main() {
    srand(time(NULL)); // Inicializa la semilla para números aleatorios
    initializeBoard();
    gameControl.score = 0;

    char choice;
    showMenu();
    choice = _getch();

    pthread_t dropThread, inputThread;

    if (choice == '1') {
        gameControl.isPlayerMode = true; // Modo jugador
        pthread_create(&dropThread, NULL, pieceDropper, NULL);
        pthread_create(&inputThread, NULL, inputHandler, NULL);
    } else if (choice == '2') {
        gameControl.isPlayerMode = false; // Modo computadora
        pthread_create(&dropThread, NULL, computerMode, NULL);
    } else {
        return 0; // Salir del programa
    }

    pthread_join(dropThread, NULL);
    pthread_join(inputThread, NULL);

    printf("¡Juego Terminado! Tu puntuación fue: %d\n", gameControl.score);
    return 0;
}