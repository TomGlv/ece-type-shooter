#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <allegro.h>

// Définitions des constantes du jeu
#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 900
#define MAX_ENEMIES 10
#define MAX_BULLETS 20
#define PLAYER_SPEED 8
#define ENEMY_SPEED 3
#define BULLET_SPEED 12
#define BACKGROUND_SCROLL_SPEED 2

// Structures pour les objets du jeu
typedef struct {
    float x, y;
    int width, height;
    int speed;
    int health;
    BITMAP* sprite;
} Player;

typedef struct {
    float x, y;
    int width, height;
    int speed;
    int health;
    int active;
    int direction;
    int shoot_cooldown;
    BITMAP* sprite;
} Enemy;

typedef struct {
    float x, y;
    int width, height;
    int speed;
    int active;
    int direction;  // 1 pour joueur, -1 pour ennemi
    BITMAP* sprite;
} Bullet;

// Variables globales
Player player;
Enemy enemies[MAX_ENEMIES];
Bullet bullets[MAX_BULLETS];
int score = 0;
int game_over = 0;
BITMAP* buffer;
BITMAP* background;
BITMAP* collision_map;
FONT* game_font;
int bg_scroll_x = 0; // Position de défilement du fond

// Prototypes de fonctions
void init();
void update();
void draw();
void cleanup();
void init_player();
void init_enemies();
void update_player();
void update_enemies();
void update_bullets();
void check_collisions();
void fire_bullet(float x, float y, int direction);
int check_collision(float x1, float y1, int w1, int h1, float x2, float y2, int w2, int h2);
int point_collision(int px, int py);

// --- FONCTION UTILITAIRE AJOUTÉE ---
int point_collision(int px, int py) {
    int map_x = (px + bg_scroll_x) % collision_map->w;
    if (map_x < 0) map_x += collision_map->w;
    if (py < 0 || py >= collision_map->h) return 0;

    int pixel = getpixel(collision_map, map_x, py);
    return (getr(pixel) == 250 && getg(pixel) == 250 && getb(pixel) == 100);
}

// Fonction principale
int main() {
    allegro_init();

    install_keyboard();
    install_timer();

    // Configuration graphique
    set_color_depth(32);
    if (set_gfx_mode(GFX_AUTODETECT, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0) != 0) {
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0);
    }

    // Initialisation du générateur de nombres aléatoires
    srand(time(NULL));

    // Création du buffer pour le double buffering
    buffer = create_bitmap(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Chargement du fond
    background = load_bitmap("background.bmp", NULL);
    if (!background) {
        // Création d'un fond par défaut
        background = create_bitmap(SCREEN_WIDTH * 2, SCREEN_HEIGHT);
        clear_to_color(background, makecol(0, 0, 50));

        // Ajout d'étoiles
        for (int i = 0; i < 1000; i++) {
            int star_x = rand() % (SCREEN_WIDTH * 2);
            int star_y = rand() % SCREEN_HEIGHT;
            int brightness = 100 + rand() % 155;
            putpixel(background, star_x, star_y, makecol(brightness, brightness, brightness));
        }
    }

    // Création d'un nouveau bitmap pour le fond étiré
    int stretched_width = background->w * 6; // Augmenté à 6x pour un fond plus étiré horizontalement
    int stretched_height = SCREEN_HEIGHT;

    BITMAP* stretched_background = create_bitmap(stretched_width, stretched_height);
    stretch_blit(background, stretched_background,
        0, 0, background->w, background->h,
        0, 0, stretched_width, stretched_height);

    // Libération du bitmap original
    destroy_bitmap(background);
    background = stretched_background;

    // Chargement de la carte de collision
    collision_map = load_bitmap("collisions.bmp", NULL);
    if (!collision_map) {
        // Création d'une carte de collision vide
        collision_map = create_bitmap(background->w, background->h);
        clear_to_color(collision_map, makecol(0, 0, 0));
    }
    else {
        // Étirer la carte de collision de la même façon que le fond
        BITMAP* stretched_collision = create_bitmap(stretched_width, stretched_height);
        stretch_blit(collision_map, stretched_collision,
            0, 0, collision_map->w, collision_map->h,
            0, 0, stretched_width, stretched_height);

        destroy_bitmap(collision_map);
        collision_map = stretched_collision;
    }

    // Tentative de chargement du sprite du joueur
    player.sprite = load_bitmap("sous-marin.bmp", NULL);
    if (player.sprite) {
        player.width = player.sprite->w * 3;
        player.height = player.sprite->h * 3;
    }
    else {
        // Création d'un sprite par défaut
        player.width = 35;
        player.height = 25;
        player.sprite = create_bitmap(player.width, player.height);
        clear_to_color(player.sprite, makecol(0, 0, 0));
        triangle(player.sprite, 0, player.height / 2, player.width, 0, player.width, player.height, makecol(0, 200, 0));
        line(player.sprite, player.width - 10, 5, player.width, 5, makecol(255, 0, 0));
        line(player.sprite, player.width - 10, player.height - 5, player.width, player.height - 5, makecol(255, 0, 0));
    }

    // Initialisation du joueur
    player.x = 100;
    player.y = SCREEN_HEIGHT / 2 - player.height / 2;
    player.speed = PLAYER_SPEED;
    player.health = 3;

    // Initialisation des ennemis
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].width = 60;
        enemies[i].height = 40;
        enemies[i].x = SCREEN_WIDTH * 0.7 + rand() % (SCREEN_WIDTH / 4);
        enemies[i].y = 100 + rand() % (SCREEN_HEIGHT - 200);
        enemies[i].speed = ENEMY_SPEED;
        enemies[i].health = 1;
        enemies[i].active = 1;
        enemies[i].direction = (rand() % 2) * 2 - 1; // -1 ou 1
        enemies[i].shoot_cooldown = 30 + rand() % 90;

        // Création du sprite ennemi
        enemies[i].sprite = create_bitmap(enemies[i].width, enemies[i].height);
        clear_to_color(enemies[i].sprite, makecol(0, 0, 0));
        triangle(enemies[i].sprite, enemies[i].width, enemies[i].height / 2, 0, 0, 0, enemies[i].height, makecol(200, 0, 0));
        circle(enemies[i].sprite, enemies[i].width / 2, enemies[i].height / 2, 10, makecol(255, 255, 0));
    }

    // Initialisation des balles
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].sprite = NULL;
    }

    // Boucle principale du jeu
    int key_pressed = 0;
    int debug_mode = 0;

    while (!key[KEY_ESC] && !game_over) {
        // Mode débogage
        if (key[KEY_D] && !key_pressed) {
            debug_mode = !debug_mode;
            key_pressed = 1;
        }
        else if (!key[KEY_D]) {
            key_pressed = 0;
        }

        // *** NOUVELLE LOGIQUE DE COLLISION ***
        int dx = 0;
        int dy = 0;
        if (key[KEY_UP] && player.y > 0) dy = -player.speed;
        if (key[KEY_DOWN] && player.y < SCREEN_HEIGHT - player.height) dy = player.speed;
        if (key[KEY_LEFT] && player.x > 0) dx = -player.speed;
        if (key[KEY_RIGHT] && player.x < SCREEN_WIDTH - player.width) dx = player.speed;

        // Vérification collisions sur 4 points
        int can_move_vertical = 1;
        int can_move_horizontal = 1;

        // Test vertical
        if (dy != 0) {
            int new_y = player.y + dy;
            int test_y = (dy > 0) ? new_y + player.height - 1 : new_y;

            if (point_collision(player.x, test_y) ||
                point_collision(player.x + player.width - 1, test_y)) {
                can_move_vertical = 0;
            }
        }

        // Test horizontal
        if (dx != 0) {
            int new_x = player.x + dx;
            int test_x = (dx > 0) ? new_x + player.width - 1 : new_x;

            if (point_collision(test_x, player.y) ||
                point_collision(test_x, player.y + player.height - 1)) {
                can_move_horizontal = 0;
            }
        }

        // Application des mouvements
        if (can_move_vertical) player.y += dy;
        if (can_move_horizontal) player.x += dx;

        // Tir
        static int fire_cooldown = 0;
        if (key[KEY_SPACE] && fire_cooldown <= 0) {
            fire_bullet(player.x + player.width, player.y + player.height / 2, 1);
            fire_cooldown = 15;
        }

        if (fire_cooldown > 0) {
            fire_cooldown--;
        }

        // Mise à jour des ennemis
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                enemies[i].y += enemies[i].direction * enemies[i].speed;

                if (enemies[i].y <= 0 || enemies[i].y >= SCREEN_HEIGHT - enemies[i].height) {
                    enemies[i].direction *= -1;
                }

                // Déplacement horizontal vers le joueur
                if (enemies[i].x > player.x + 100) {
                    enemies[i].x -= enemies[i].speed / 2;
                }
            }
        }

        // Mise à jour des balles
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].active) {
                bullets[i].x += bullets[i].speed * bullets[i].direction;

                if (bullets[i].x < 0 || bullets[i].x > SCREEN_WIDTH) {
                    bullets[i].active = 0;
                }
            }
        }

        // Vérification des collisions balles-ennemis
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].active && bullets[i].direction == 1) { // Balles du joueur
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (enemies[j].active) {
                        if (check_collision(bullets[i].x, bullets[i].y, bullets[i].width, bullets[i].height,
                            enemies[j].x, enemies[j].y, enemies[j].width, enemies[j].height)) {
                            enemies[j].health--;
                            bullets[i].active = 0;

                            if (enemies[j].health <= 0) {
                                enemies[j].active = 0;
                                score += 100;

                                // Respawn de l'ennemi à droite de l'écran après un certain temps
                                if (rand() % 100 < 80) { // 80% de chance de respawn
                                    enemies[j].x = SCREEN_WIDTH + rand() % 100;
                                    enemies[j].y = 100 + rand() % (SCREEN_HEIGHT - 200);
                                    enemies[j].health = 1;
                                    enemies[j].active = 1;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        // Vérification collision joueur-ennemi
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                if (check_collision(player.x, player.y, player.width, player.height,
                    enemies[i].x, enemies[i].y, enemies[i].width, enemies[i].height)) {
                    player.health--;
                    enemies[i].active = 0;

                    if (player.health <= 0) {
                        game_over = 1;
                    }
                    break;
                }
            }
        }

        // Défilement du fond - comme dans l'exemple fourni
        bg_scroll_x += BACKGROUND_SCROLL_SPEED;
        if (bg_scroll_x >= background->w - SCREEN_WIDTH) {
            bg_scroll_x = 0;
        }

        // Dessin
        clear_bitmap(buffer);

        // Dessin du fond avec défilement - comme dans l'exemple fourni
        int display_y = 0;
        blit(background, buffer, bg_scroll_x, display_y, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        // Mode débogage
        if (debug_mode) {
            // Afficher les zones de collision en rouge
            for (int x = 0; x < SCREEN_WIDTH; x += 10) {
                for (int y = 0; y < SCREEN_HEIGHT; y += 10) {
                    if (point_collision(x, y)) {
                        circlefill(buffer, x, y, 2, makecol(255, 0, 0));
                    }
                }
            }

            // Afficher les points de vérification de collision
            if (dy != 0) {
                int test_y = (dy > 0) ? player.y + player.height - 1 : player.y;
                circlefill(buffer, player.x, test_y, 3, makecol(0, 255, 0));
                circlefill(buffer, player.x + player.width - 1, test_y, 3, makecol(0, 255, 0));
            }

            if (dx != 0) {
                int test_x = (dx > 0) ? player.x + player.width - 1 : player.x;
                circlefill(buffer, test_x, player.y, 3, makecol(0, 0, 255));
                circlefill(buffer, test_x, player.y + player.height - 1, 3, makecol(0, 0, 255));
            }

            // Afficher infos débogage
            textprintf_ex(buffer, font, 20, 80, makecol(255, 255, 0), -1,
                "DEBUG: Player(%d,%d) Scroll:%d", (int)player.x, (int)player.y, bg_scroll_x);
            textprintf_ex(buffer, font, 20, 100, makecol(255, 255, 0), -1,
                "Move: V=%d H=%d", can_move_vertical, can_move_horizontal);
        }

        // Dessin du joueur
        stretch_sprite(buffer, player.sprite, player.x, player.y, player.width, player.height);

        // Dessin des ennemis
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                draw_sprite(buffer, enemies[i].sprite, enemies[i].x, enemies[i].y);
            }
        }

        // Dessin des balles
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].active && bullets[i].sprite) {
                draw_sprite(buffer, bullets[i].sprite, bullets[i].x, bullets[i].y);
            }
        }

        // Affichage du score et de la vie
        textprintf_ex(buffer, font, 20, 20, makecol(255, 255, 255), -1, "Score: %d", score);
        textprintf_ex(buffer, font, 20, 50, makecol(255, 255, 255), -1, "Vie: %d", player.health);

        // Affichage Game Over
        if (game_over) {
            textprintf_centre_ex(buffer, font, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                makecol(255, 0, 0), -1, "GAME OVER - Score final: %d", score);
            textprintf_centre_ex(buffer, font, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30,
                makecol(255, 255, 255), -1, "Appuyez sur ESC pour quitter");
        }

        // Transfert du buffer à l'écran
        blit(buffer, screen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        rest(16); // Environ 60 FPS
    }

    // Nettoyage
    destroy_bitmap(buffer);
    destroy_bitmap(background);
    destroy_bitmap(collision_map);
    destroy_bitmap(player.sprite);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].sprite) {
            destroy_bitmap(enemies[i].sprite);
        }
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].sprite) {
            destroy_bitmap(bullets[i].sprite);
        }
    }

    return 0;
}

// Fonction pour créer et activer une balle
void fire_bullet(float x, float y, int direction) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].width = 15;
            bullets[i].height = 6;
            bullets[i].speed = BULLET_SPEED;
            bullets[i].active = 1;
            bullets[i].direction = direction;

            // Création du sprite de la balle si nécessaire
            if (!bullets[i].sprite) {
                bullets[i].sprite = create_bitmap(bullets[i].width, bullets[i].height);
                if (!bullets[i].sprite) {
                    // En cas d'échec, désactiver la balle
                    bullets[i].active = 0;
                    return;
                }

                clear_to_color(bullets[i].sprite, makecol(0, 0, 0));
                int color = (direction == 1) ? makecol(0, 255, 255) : makecol(255, 100, 0);
                rectfill(bullets[i].sprite, 0, 0, bullets[i].width, bullets[i].height, color);
            }

            break;
        }
    }
}

// Fonction utilitaire pour vérifier une collision entre deux rectangles
int check_collision(float x1, float y1, int w1, int h1, float x2, float y2, int w2, int h2) {
    return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

END_OF_MAIN()