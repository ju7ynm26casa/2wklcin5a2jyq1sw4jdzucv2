#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <math.h>

#define RST "\033[0m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YEL "\033[33m"
#define BLU "\033[34m"
#define MAG "\033[35m"
#define CYN "\033[36m"
#define WHT "\033[37m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define ITAL "\033[3m"
#define BRED "\033[91m"
#define BGRN "\033[92m"
#define BYEL "\033[93m"
#define BBLU "\033[94m"
#define BMAG "\033[95m"
#define BCYN "\033[96m"
#define BWHT "\033[97m"

#define MAX_INV 24
#define MAX_ROOMS 28
#define MAX_ENEMIES 18
#define MAX_NPCS 8

typedef struct {
    char name[64];
    int atk, def, max_hp, hp, xp, level, gold;
    int inv_count;
    char inv[MAX_INV][64];
    int has_sword, has_shield, has_amulet, has_key, has_crystal, has_crown;
    int has_dragon_heart, has_torch;
    int crit_chance, dodge_chance;
    int special_ready;
    int rested[MAX_ROOMS];
} Player;

typedef struct {
    char name[64];
    int atk, def, max_hp, hp, xp_r, gold_r;
    char color[16];
    char art[8][50];
    int art_lines;
    int is_boss;
    int phase;
    int drops;
    char drop_name[64];
    int special_round;
    char special_msg[128];
} Enemy;

typedef struct {
    char name[64];
    char desc[512];
    char art[14][80];
    int art_lines;
    int n, s, e, w;
    int enemy_id, npc_id;
    int has_item;
    char item_name[64];
    int zone;
    int visited;
    int is_heal;
    int has_treasure;
} Room;

typedef struct {
    char name[64];
    char lines[6][256];
    int count;
    int gives;
    char give_name[64];
    char color[16];
    int is_shop;
} NPC;

Player P;
Room R[MAX_ROOMS];
Enemy E[MAX_ENEMIES];
NPC N[MAX_NPCS];
int cur_room, game_over, in_combat, cur_enemy, game_won;
time_t start_time;
int interactive_mode = 1;
int explored_count = 0;
int enemies_defeated = 0;
int total_damage_dealt = 0;
int total_damage_taken = 0;
int crit_count = 0;
int dodge_count = 0;
int rooms_visited[28];
int last_zone = -1;

void clr() { printf("\033[2J\033[H"); }
void gotoxy(int x, int y) { printf("\033[%d;%dH", y, x); }

void type(const char *t, int speed) {
    for (int i = 0; t[i]; i++) { putchar(t[i]); fflush(stdout); usleep(speed); }
    printf("\n");
}
void type_slow(const char *t) { type(t, 18000); }
void type_fast(const char *t) { type(t, 3000); }

void sep() { printf(DIM "──────────────────────────────────────────────────────────────────────────" RST "\n"); }
void sep2() { printf(BBLU "════════════════════════════════════════════════════════════════════════════" RST "\n"); }
void sep3() { printf(BRED "▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" RST "\n"); }

void hp_bar(int hp, int mx, int width) {
    int f = (hp * width) / mx;
    printf("[");
    for (int i = 0; i < width; i++) {
        if (i < f) {
            int pct = hp * 100 / mx;
            if (pct > 60) printf(BGRN "█" RST);
            else if (pct > 30) printf(BYEL "█" RST);
            else printf(BRED "█" RST);
        } else printf(DIM "░" RST);
    }
    printf("] %d/%d", hp, mx);
}

void loading_screen() {
    clr();
    printf("\n\n\n");
    printf(BWHT "  Loading THE SHATTERED REALM..." RST "\n\n");
    const char *msgs[] = {
        "  Generating forests...",
        "  Carving caves...",
        "  Building castles...",
        "  Summoning the Void...",
        "  Placing enemies...",
        "  Scattering treasure...",
        "  Awakening the Void King...",
        "  Ready!"
    };
    for (int i = 0; i < 8; i++) {
        printf("  %s", msgs[i]);
        fflush(stdout);
        for (int j = 0; j < 30; j++) {
            printf(BGRN "█" RST);
            fflush(stdout);
            usleep(20000);
        }
        printf(" " BGRN "✓" RST "\n");
    }
    usleep(500000);
}

void zone_transition(int zone) {
    if (zone == last_zone) return;
    last_zone = zone;
    clr();
    printf("\n\n\n\n\n");
    if (zone == 0) {
        printf(BGRN "  ═══════════════════════════════════════════════════════" RST "\n");
        printf(BGRN "       THE WHISPERING FOREST" RST "\n");
        printf(BGRN "  ═══════════════════════════════════════════════════════" RST "\n");
        type_slow("  Ancient trees whisper secrets of the old world...");
        type_slow("  Bioluminescent fungi light your path forward.");
    } else if (zone == 1) {
        printf(BCYN "  ═══════════════════════════════════════════════════════" RST "\n");
        printf(BCYN "       THE CRYSTAL CAVES" RST "\n");
        printf(BCYN "  ═══════════════════════════════════════════════════════" RST "\n");
        type_slow("  Deep below the surface, crystals sing in harmony...");
        type_slow("  But something stirs in the darkness ahead.");
    } else if (zone == 2) {
        printf(BMAG "  ═══════════════════════════════════════════════════════" RST "\n");
        printf(BMAG "       THE FORSAKEN CASTLE" RST "\n");
        printf(BMAG "  ═══════════════════════════════════════════════════════" RST "\n");
        type_slow("  Once a seat of power, now a monument to corruption...");
        type_slow("  The walls weep with the memories of the fallen.");
    } else if (zone == 3) {
        printf(BRED "  ═══════════════════════════════════════════════════════" RST "\n");
        printf(BRED "       THE VOID REALM" RST "\n");
        printf(BRED "  ═══════════════════════════════════════════════════════" RST "\n");
        type_slow("  Reality unravels at the edges of existence...");
        type_slow("  The Void King's influence corrupts all it touches.");
    }
    usleep(300000);
}

int detect_interactive() {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, 50);
    if (ret > 0 && isatty(STDIN_FILENO)) return 1;
    return 0;
}

void wait_enter() {
    if (!interactive_mode) { usleep(200000); return; }
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    poll(&pfd, 1, 5000);
    if (pfd.revents & POLLIN) getchar();
}

char get_key() {
    if (!interactive_mode) return '\0';
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, 10000);
    if (ret <= 0) return '\0';
    char c = getchar();
    while (c == '\n') c = getchar();
    return c;
}

int xp_needed(int level) {
    return (int)(50.0 * pow(1.45, level - 1));
}

void init_enemies() {
    memset(E, 0, sizeof(E));
    strcpy(E[0].name,"Shadow Wolf"); E[0].atk=9; E[0].def=3; E[0].max_hp=40; E[0].hp=40;
    E[0].xp_r=25; E[0].gold_r=15; strcpy(E[0].color,RED); E[0].art_lines=4;
    strcpy(E[0].art[0],"    /\\_/\\    "); strcpy(E[0].art[1],"   ( o.o )   ");
    strcpy(E[0].art[2],"    > ^ <    "); strcpy(E[0].art[3],"   /|   |\\   ");

    strcpy(E[1].name,"Giant Spider"); E[1].atk=11; E[1].def=3; E[1].max_hp=35; E[1].hp=35;
    E[1].xp_r=22; E[1].gold_r=12; strcpy(E[1].color,MAG); E[1].art_lines=4;
    strcpy(E[1].art[0],"  /\\  /\\  /\\ "); strcpy(E[1].art[1]," /  \\/  \\/  \\");
    strcpy(E[1].art[2],"    (o  o)   "); strcpy(E[1].art[3],"   /|    |\\  ");

    strcpy(E[2].name,"Forest Troll"); E[2].atk=14; E[2].def=6; E[2].max_hp=65; E[2].hp=65;
    E[2].xp_r=40; E[2].gold_r=30; strcpy(E[2].color,GRN); E[2].art_lines=5;
    strcpy(E[2].art[0],"   .---.    "); strcpy(E[2].art[1],"  / o o \\   ");
    strcpy(E[2].art[2],"  |  ^  |   "); strcpy(E[2].art[3],"  | \\_/ |   ");
    strcpy(E[2].art[4]," /|     |\\  ");

    strcpy(E[3].name,"Thorn Golem"); E[3].atk=12; E[3].def=10; E[3].max_hp=75; E[3].hp=75;
    E[3].xp_r=42; E[3].gold_r=28; strcpy(E[3].color,YEL); E[3].art_lines=5;
    strcpy(E[3].art[0],"  /\\ /\\ /\\  "); strcpy(E[3].art[1]," |  |  |  | ");
    strcpy(E[3].art[2]," | [o][o] | "); strcpy(E[3].art[3]," |  \\__/  | ");
    strcpy(E[3].art[4]," /__||||__\\ ");

    strcpy(E[4].name,"Cave Bat Swarm"); E[4].atk=10; E[4].def=2; E[4].max_hp=45; E[4].hp=45;
    E[4].xp_r=25; E[4].gold_r=15; strcpy(E[4].color,MAG); E[4].art_lines=3;
    strcpy(E[4].art[0],"  /\\  /\\  /\\ "); strcpy(E[4].art[1]," /  \\/  \\/  \\");
    strcpy(E[4].art[2],"  \\  /\\  /  /");

    strcpy(E[5].name,"Stone Serpent"); E[5].atk=13; E[5].def=8; E[5].max_hp=70; E[5].hp=70;
    E[5].xp_r=38; E[5].gold_r=28; strcpy(E[5].color,CYN); E[5].art_lines=4;
    strcpy(E[5].art[0],"    __       "); strcpy(E[5].art[1],"   /  \\___   ");
    strcpy(E[5].art[2],"  / o   o \\  "); strcpy(E[5].art[3],"  \\  ~~~  /  ");

    strcpy(E[6].name,"Crystal Wraith"); E[6].atk=16; E[6].def=5; E[6].max_hp=60; E[6].hp=60;
    E[6].xp_r=42; E[6].gold_r=38; strcpy(E[6].color,BCYN); E[6].art_lines=5;
    strcpy(E[6].art[0],"    /\\       "); strcpy(E[6].art[1],"   /  \\      ");
    strcpy(E[6].art[2],"  / ** \\     "); strcpy(E[6].art[3]," |  **  |    ");
    strcpy(E[6].art[4],"  \\    /     ");

    strcpy(E[7].name,"Dark Knight"); E[7].atk=19; E[7].def=12; E[7].max_hp=95; E[7].hp=95;
    E[7].xp_r=55; E[7].gold_r=50; strcpy(E[7].color,RED); E[7].art_lines=6;
    strcpy(E[7].art[0],"    /|\\      "); strcpy(E[7].art[1],"   / | \\     ");
    strcpy(E[7].art[2],"  |  |  |    "); strcpy(E[7].art[3],"  | [X] |    ");
    strcpy(E[7].art[4],"  |  |  |    "); strcpy(E[7].art[5]," /|  |  |\\   ");

    strcpy(E[8].name,"Shadow Mage"); E[8].atk=22; E[8].def=6; E[8].max_hp=70; E[8].hp=70;
    E[8].xp_r=60; E[8].gold_r=55; strcpy(E[8].color,BMAG); E[8].art_lines=5;
    strcpy(E[8].art[0],"    /\\       "); strcpy(E[8].art[1],"   /  \\      ");
    strcpy(E[8].art[2],"  | ** |     "); strcpy(E[8].art[3],"  |    |     ");
    strcpy(E[8].art[4]," /|    |\\    ");

    strcpy(E[9].name,"THE VOID KING"); E[9].atk=26; E[9].def=13; E[9].max_hp=380; E[9].hp=380;
    E[9].xp_r=400; E[9].gold_r=999; strcpy(E[9].color,BRED); E[9].is_boss=1; E[9].art_lines=7;
    E[9].special_round = 5;
    strcpy(E[9].special_msg,"VOID ERUPTION!");
    strcpy(E[9].art[0],"  \\\\  ||  //  "); strcpy(E[9].art[1],"   \\\\ || //   ");
    strcpy(E[9].art[2],"    \\\\||//    "); strcpy(E[9].art[3],"   /|    |\\   ");
    strcpy(E[9].art[4],"  | |XX  | |  "); strcpy(E[9].art[5],"  | |    | |  ");
    strcpy(E[9].art[6]," /| |    | |\\ ");

    strcpy(E[10].name,"Ancient Dragon"); E[10].atk=24; E[10].def=12; E[10].max_hp=220; E[10].hp=220;
    E[10].xp_r=180; E[10].gold_r=150; strcpy(E[10].color,BYEL); E[10].is_boss=1;
    E[10].drops=1; strcpy(E[10].drop_name,"Dragon Heart"); E[10].art_lines=6;
    E[10].special_round = 4;
    strcpy(E[10].special_msg,"FIRE BREATH!");
    strcpy(E[10].art[0],"        /\\  "); strcpy(E[10].art[1],"       /  \\ ");
    strcpy(E[10].art[2],"  /\\  / ** \\"); strcpy(E[10].art[3]," /  \\/  ~~  ");
    strcpy(E[10].art[4],"/    \\_____/"); strcpy(E[10].art[5],"\\   /|     |");

    strcpy(E[11].name,"Void Sentinel"); E[11].atk=18; E[11].def=9; E[11].max_hp=80; E[11].hp=80;
    E[11].xp_r=70; E[11].gold_r=60; strcpy(E[11].color,BMAG); E[11].art_lines=5;
    strcpy(E[11].art[0],"   /====\\   "); strcpy(E[11].art[1],"  |  ##  |  ");
    strcpy(E[11].art[2],"  | ##  |   "); strcpy(E[11].art[3],"  |  ##  |  ");
    strcpy(E[11].art[4]," /|      |\\ ");

    strcpy(E[12].name,"Flame Imp"); E[12].atk=12; E[12].def=3; E[12].max_hp=40; E[12].hp=40;
    E[12].xp_r=18; E[12].gold_r=14; strcpy(E[12].color,BYEL); E[12].art_lines=3;
    strcpy(E[12].art[0],"   /\\ /\\    "); strcpy(E[12].art[1],"  | o o |   ");
    strcpy(E[12].art[2],"  |  ~  |   ");

    strcpy(E[13].name,"Ancient Guardian"); E[13].atk=22; E[13].def=14; E[13].max_hp=200; E[13].hp=200;
    E[13].xp_r=150; E[13].gold_r=110; strcpy(E[13].color,BGRN); E[13].is_boss=1;
    E[13].drops=1; strcpy(E[13].drop_name,"Guardian's Key"); E[13].art_lines=6;
    E[13].special_round = 5;
    strcpy(E[13].special_msg,"SEISMIC SLAM!");
    strcpy(E[13].art[0],"   /====\\   "); strcpy(E[13].art[1],"  |  @@  |  ");
    strcpy(E[13].art[2],"  |      |  "); strcpy(E[13].art[3]," /|      |\\ ");
    strcpy(E[13].art[4],"| |      | |"); strcpy(E[13].art[5]," \\|      |/ ");

    strcpy(E[14].name,"Cursed Knight"); E[14].atk=17; E[14].def=10; E[14].max_hp=80; E[14].hp=80;
    E[14].xp_r=48; E[14].gold_r=40; strcpy(E[14].color,CYN); E[14].art_lines=5;
    strcpy(E[14].art[0],"    /|\\      "); strcpy(E[14].art[1],"   / | \\     ");
    strcpy(E[14].art[2],"  |  |  |    "); strcpy(E[14].art[3],"  | [~] |    ");
    strcpy(E[14].art[4]," /|  |  |\\   ");

    strcpy(E[15].name,"Bone Colossus"); E[15].atk=20; E[15].def=11; E[15].max_hp=120; E[15].hp=120;
    E[15].xp_r=95; E[15].gold_r=75; strcpy(E[15].color,WHT); E[15].art_lines=6;
    strcpy(E[15].art[0],"   .===.    "); strcpy(E[15].art[1],"  / o o \\   ");
    strcpy(E[15].art[2],"  |  ^  |   "); strcpy(E[15].art[3]," /|     |\\  ");
    strcpy(E[15].art[4],"| |     | | "); strcpy(E[15].art[5]," \\|     |/  ");

    strcpy(E[16].name,"Void Stalker"); E[16].atk=24; E[16].def=8; E[16].max_hp=95; E[16].hp=95;
    E[16].xp_r=100; E[16].gold_r=80; strcpy(E[16].color,BMAG); E[16].art_lines=5;
    strcpy(E[16].art[0],"   /\\  /\\   "); strcpy(E[16].art[1],"  /  \\/  \\  ");
    strcpy(E[16].art[2]," |  **  |   "); strcpy(E[16].art[3],"  \\    /    ");
    strcpy(E[16].art[4],"  /|  |\\    ");

    strcpy(E[17].name,"Elder Wyrm"); E[17].atk=26; E[17].def=13; E[17].max_hp=170; E[17].hp=170;
    E[17].xp_r=200; E[17].gold_r=160; strcpy(E[17].color,BRED); E[17].is_boss=1;
    E[17].special_round = 4;
    strcpy(E[17].special_msg,"ANCIENT ROAR!");
    strcpy(E[17].art[0],"     /\\     "); strcpy(E[17].art[1],"    /  \\    ");
    strcpy(E[17].art[2],"   / ** \\   "); strcpy(E[17].art[3],"  /  ~~  \\  ");
    strcpy(E[17].art[4]," /________\\ "); strcpy(E[17].art[5]," \\  |  |  / ");
}

void init_npcs() {
    memset(N, 0, sizeof(N));
    strcpy(N[0].name,"Elder Willow"); strcpy(N[0].color,BGRN); N[0].count=5;
    strcpy(N[0].lines[0],"\"Ah, brave soul! The realm is shattered...");
    strcpy(N[0].lines[1]," The Void King broke the Crystal of Unity,");
    strcpy(N[0].lines[2]," splitting our world into fragments.");
    strcpy(N[0].lines[3]," Journey through Caves, Castle, and the Void!");
    strcpy(N[0].lines[4]," Grow strong. Find the Crown. End this.\"");
    N[0].gives=1; strcpy(N[0].give_name,"Healing Potion");

    strcpy(N[1].name,"Wandering Merchant"); strcpy(N[1].color,BYEL); N[1].count=3;
    strcpy(N[1].lines[0],"\"Welcome, traveler! I sell wares for the journey.");
    strcpy(N[1].lines[1]," A fine sword for 30 gold? A shield for 25?");
    strcpy(N[1].lines[2]," Or perhaps a potion for 10 gold?\"");
    N[1].is_shop=1;

    strcpy(N[2].name,"Crystal Hermit"); strcpy(N[2].color,BCYN); N[2].count=4;
    strcpy(N[2].lines[0],"\"The crystals here hold ancient power...");
    strcpy(N[2].lines[1]," I have studied them for a hundred years.");
    strcpy(N[2].lines[2]," Take this amulet. It will protect you");
    strcpy(N[2].lines[3]," from the Void's corruption.\"");
    N[2].gives=1; strcpy(N[2].give_name,"Crystal Amulet");

    strcpy(N[3].name,"Ghost of the King"); strcpy(N[3].color,WHT); N[3].count=5;
    strcpy(N[3].lines[0],"\"I was the king... before the Void consumed me.");
    strcpy(N[3].lines[1]," My crown holds the last light of this realm.");
    strcpy(N[3].lines[2]," Take it. End what I could not.");
    strcpy(N[3].lines[3]," The Void King fears the Crown of Dawn...");
    strcpy(N[3].lines[4]," Use it wisely, hero.\"");
    N[3].gives=1; strcpy(N[3].give_name,"Crown of Dawn");

    strcpy(N[4].name,"The Oracle"); strcpy(N[4].color,BMAG); N[4].count=4;
    strcpy(N[4].lines[0],"\"You stand at the edge of oblivion...");
    strcpy(N[4].lines[1]," The Void King awaits in his throne room.");
    strcpy(N[4].lines[2]," Only the Crown and Crystal can save us.");
    strcpy(N[4].lines[3]," But beware... he has grown stronger.\"");

    strcpy(N[5].name,"Dying Warrior"); strcpy(N[5].color,BRED); N[5].count=4;
    strcpy(N[5].lines[0],"\"I... tried to reach the Void King...");
    strcpy(N[5].lines[1]," He is too powerful... without the Crown...");
    strcpy(N[5].lines[2]," Take my key... open the final gate...");
    strcpy(N[5].lines[3]," Avenge us all...\"");
    N[5].gives=1; strcpy(N[5].give_name,"Void Key");

    strcpy(N[6].name,"Lost Scholar"); strcpy(N[6].color,BBLU); N[6].count=4;
    strcpy(N[6].lines[0],"\"I've been trapped in these caves for years...");
    strcpy(N[6].lines[1]," The dragon guards something precious.");
    strcpy(N[6].lines[2]," Its heart holds the power to heal the Crystal.");
    strcpy(N[6].lines[3]," Defeat it, and take its heart.\"");

    strcpy(N[7].name,"Spirit Guide"); strcpy(N[7].color,BCYN); N[7].count=3;
    strcpy(N[7].lines[0],"\"I am the spirit of this forest...");
    strcpy(N[7].lines[1]," The Void corrupts all it touches.");
    strcpy(N[7].lines[2]," You must purify the realm, hero.\"");
}

void init_rooms() {
    memset(R, 0, sizeof(R));
    memset(rooms_visited, 0, sizeof(rooms_visited));

    strcpy(R[0].name,"The Whispering Woods");
    strcpy(R[0].desc,"Ancient trees tower above. Bioluminescent mushrooms glow blue. A path leads deeper.");
    R[0].n=1; R[0].s=-1; R[0].e=2; R[0].w=-1;
    R[0].enemy_id=-1; R[0].npc_id=0; R[0].zone=0;
    R[0].art_lines=8;
    strcpy(R[0].art[0],"        *  .  *        .        *     .    *       ");
    strcpy(R[0].art[1],"   .       *      .        *       .        *       ");
    strcpy(R[0].art[2],"      /\\      /\\        /\\      /\\        /\\       ");
    strcpy(R[0].art[3],"     /  \\    /  \\      /  \\    /  \\      /  \\      ");
    strcpy(R[0].art[4],"    /    \\  /    \\    /    \\  /    \\    /    \\     ");
    strcpy(R[0].art[5],"   /      \\/      \\  /      \\/      \\  /      \\    ");
    strcpy(R[0].art[6],"  /   *    *   *   \\/   *    *   *   \\/   *    *   ");
    strcpy(R[0].art[7],"  \\   *    *   *   /\\   *    *   *   /\\   *    *   ");

    strcpy(R[1].name,"Mossy Clearing");
    strcpy(R[1].desc,"Golden light filters through. A rune is carved in a standing stone. Something rustles...");
    R[1].n=3; R[1].s=0; R[1].e=-1; R[1].w=-1;
    R[1].enemy_id=0; R[1].npc_id=-1; R[1].zone=0;
    R[1].has_item=1; strcpy(R[1].item_name,"Healing Potion");
    R[1].art_lines=5;
    strcpy(R[1].art[0],"    *    .    *    .    *    .    *    .    ");
    strcpy(R[1].art[1],"         .  *  .  *  .  *  .  *  .  *  .          ");
    strcpy(R[1].art[2],"    ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~      ");
    strcpy(R[1].art[3],"         .  *  .  *  .  *  .  *  .  *  .          ");
    strcpy(R[1].art[4],"    *    .    *    .    *    .    *    .    ");

    strcpy(R[2].name,"The Webbed Hollow");
    strcpy(R[2].desc,"Thick webs stretch between trees. Clicking sounds echo from above.");
    R[2].n=-1; R[2].s=-1; R[2].e=4; R[2].w=0;
    R[2].enemy_id=1; R[2].npc_id=1; R[2].zone=0;
    R[2].art_lines=5;
    strcpy(R[2].art[0],"  \\  |  /  \\  |  /  \\  |  /  \\  |  /  \\  |  /  ");
    strcpy(R[2].art[1],"   \\ | /    \\ | /    \\ | /    \\ | /    \\ | /   ");
    strcpy(R[2].art[2],"    \\|/      \\|/      \\|/      \\|/      \\|/    ");
    strcpy(R[2].art[3],"     *        *        *        *        *      ");
    strcpy(R[2].art[4],"    /|\\      /|\\      /|\\      /|\\      /|\\    ");

    strcpy(R[3].name,"The Troll Bridge");
    strcpy(R[3].desc,"A rickety bridge spans a deep ravine. A massive figure blocks the far end.");
    R[3].n=5; R[3].s=1; R[3].e=-1; R[3].w=-1;
    R[3].enemy_id=2; R[3].npc_id=-1; R[3].zone=0;
    R[3].art_lines=5;
    strcpy(R[3].art[0],"  |\\  |  /|  |\\  |  /|  |\\  |  /|  |\\  |  /|  ");
    strcpy(R[3].art[1],"  | \\ | / |  | \\ | / |  | \\ | / |  | \\ | / |  ");
    strcpy(R[3].art[2],"  |  \\|/  |  |  \\|/  |  |  \\|/  |  |  \\|/  |  ");
    strcpy(R[3].art[3],"  |   |   |  |   |   |  |   |   |  |   |   |  ");
    strcpy(R[3].art[4],"  |___|___|__|___|___|__|___|___|__|___|___|  ");

    strcpy(R[4].name,"The Thorn Maze");
    strcpy(R[4].desc,"Living thorns form a maze. A golem guards the passage to the caves.");
    R[4].n=-1; R[4].s=-1; R[4].e=5; R[4].w=2;
    R[4].enemy_id=3; R[4].npc_id=-1; R[4].zone=0;
    R[4].art_lines=5;
    strcpy(R[4].art[0],"  #  #  #  #  #  #  #  #  #  #  #  #  #  #  #  ");
    strcpy(R[4].art[1],"  #        #        #        #        #        #  ");
    strcpy(R[4].art[2],"  #  #  #  #  #  #  #  #  #  #  #  #  #  #  #  #  ");
    strcpy(R[4].art[3],"  #        #        #        #        #        #  ");
    strcpy(R[4].art[4],"  #  #  #  #  #  #  #  #  #  #  #  #  #  #  #  #  ");

    strcpy(R[5].name,"The Cave Mouth");
    strcpy(R[5].desc,"A gaping maw of darkness. Cold air rushes out. Crystals glitter in torchlight.");
    R[5].n=6; R[5].s=3; R[5].e=4; R[5].w=-1;
    R[5].enemy_id=-1; R[5].npc_id=-1; R[5].zone=1;
    R[5].has_item=1; strcpy(R[5].item_name,"Torch");
    R[5].art_lines=5;
    strcpy(R[5].art[0],"  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  ");
    strcpy(R[5].art[1]," /  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\ ");
    strcpy(R[5].art[2],"/    \\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\    \\");
    strcpy(R[5].art[3]," \\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  /\\  / ");
    strcpy(R[5].art[4],"  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  ");

    strcpy(R[6].name,"Crystal Tunnels");
    strcpy(R[6].desc,"Tunnels sparkle with embedded crystals. Water drips rhythmically. Something skitters ahead.");
    R[6].n=7; R[6].s=5; R[6].e=-1; R[6].w=-1;
    R[6].enemy_id=4; R[6].npc_id=2; R[6].zone=1;
    R[6].art_lines=5;
    strcpy(R[6].art[0],"  |  *  |  *  |  *  |  *  |  *  |  *  |  *  |  ");
    strcpy(R[6].art[1],"  |     |     |     |     |     |     |     |  ");
    strcpy(R[6].art[2],"  |  *  |  *  |  *  |  *  |  *  |  *  |  *  |  ");
    strcpy(R[6].art[3],"  |     |     |     |     |     |     |     |  ");
    strcpy(R[6].art[4],"  |  *  |  *  |  *  |  *  |  *  |  *  |  *  |  ");

    strcpy(R[7].name,"The Serpent's Lair");
    strcpy(R[7].desc,"A vast chamber. Bones litter the floor. A stone serpent coils around a pillar.");
    R[7].n=8; R[7].s=6; R[7].e=-1; R[7].w=-1;
    R[7].enemy_id=5; R[7].npc_id=6; R[7].zone=1;
    R[7].art_lines=5;
    strcpy(R[7].art[0],"  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~");
    strcpy(R[7].art[1],"  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~");
    strcpy(R[7].art[2],"  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~");
    strcpy(R[7].art[3],"  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~");
    strcpy(R[7].art[4],"  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~  ~~~~");

    strcpy(R[8].name,"The Crystal Chamber");
    strcpy(R[8].desc,"Massive crystals pulse with inner light. A wraith materializes from the glow!");
    R[8].n=9; R[8].s=7; R[8].e=-1; R[8].w=-1;
    R[8].enemy_id=6; R[8].npc_id=-1; R[8].zone=1;
    R[8].has_item=1; strcpy(R[8].item_name,"Crystal Shard");
    R[8].art_lines=5;
    strcpy(R[8].art[0],"    /\\    /\\    /\\    /\\    /\\    /\\    /\\    ");
    strcpy(R[8].art[1],"   /  \\  /  \\  /  \\  /  \\  /  \\  /  \\  /  \\   ");
    strcpy(R[8].art[2],"  /    \\/    \\/    \\/    \\/    \\/    \\/    \\  ");
    strcpy(R[8].art[3],"   \\  /  \\  /  \\  /  \\  /  \\  /  \\  /  \\  /   ");
    strcpy(R[8].art[4],"    \\/    \\/    \\/    \\/    \\/    \\/    \\/    ");

    strcpy(R[9].name,"The Castle Gate");
    strcpy(R[9].desc,"A towering gate of black iron. The castle looms above, wreathed in dark clouds.");
    R[9].n=10; R[9].s=8; R[9].e=-1; R[9].w=-1;
    R[9].enemy_id=-1; R[9].npc_id=-1; R[9].zone=2;
    R[9].art_lines=6;
    strcpy(R[9].art[0],"  /\\        /\\        /\\        /\\        /\\  ");
    strcpy(R[9].art[1]," /  \\      /  \\      /  \\      /  \\      /  \\ ");
    strcpy(R[9].art[2],"|    |    |    |    |    |    |    |    |    |");
    strcpy(R[9].art[3],"|    |____|    |____|    |____|    |____|    |");
    strcpy(R[9].art[4],"|    |    |    |    |    |    |    |    |    |");
    strcpy(R[9].art[5],"|____|____|____|____|____|____|____|____|____|");

    strcpy(R[10].name,"The Grand Hall");
    strcpy(R[10].desc,"Tattered banners hang from vaulted ceilings. A Dark Knight stands guard.");
    R[10].n=11; R[10].s=9; R[10].e=-1; R[10].w=-1;
    R[10].enemy_id=7; R[10].npc_id=-1; R[10].zone=2;
    R[10].art_lines=5;
    strcpy(R[10].art[0],"  |\\  |  /|  |\\  |  /|  |\\  |  /|  |\\  |  /|  ");
    strcpy(R[10].art[1],"  | \\ | / |  | \\ | / |  | \\ | / |  | \\ | / |  ");
    strcpy(R[10].art[2],"  |  \\|/  |  |  \\|/  |  |  \\|/  |  |  \\|/  |  ");
    strcpy(R[10].art[3],"  |   |   |  |   |   |  |   |   |  |   |   |  ");
    strcpy(R[10].art[4],"  |___|___|__|___|___|__|___|___|__|___|___|  ");

    strcpy(R[11].name,"The Throne Room");
    strcpy(R[11].desc,"A ghostly king sits upon a broken throne. His eyes glow with sorrow.");
    R[11].n=12; R[11].s=10; R[11].e=-1; R[11].w=-1;
    R[11].enemy_id=-1; R[11].npc_id=3; R[11].zone=2;
    R[11].art_lines=5;
    strcpy(R[11].art[0],"    /====\\    /====\\    /====\\    /====\\    ");
    strcpy(R[11].art[1],"   |      |  |      |  |      |  |      |   ");
    strcpy(R[11].art[2],"   |  @@  |  |      |  |  @@  |  |      |   ");
    strcpy(R[11].art[3],"   |______|  |______|  |______|  |______|   ");
    strcpy(R[11].art[4],"  /========\\/========\\/========\\/========\\  ");

    strcpy(R[12].name,"The Dark Tower");
    strcpy(R[12].desc,"A spiraling staircase leads up. A Shadow Mage cackles at the top.");
    R[12].n=13; R[12].s=11; R[12].e=-1; R[12].w=-1;
    R[12].enemy_id=8; R[12].npc_id=-1; R[12].zone=2;
    R[12].art_lines=5;
    strcpy(R[12].art[0],"      /\\      ");
    strcpy(R[12].art[1],"     /  \\     ");
    strcpy(R[12].art[2],"    /    \\    ");
    strcpy(R[12].art[3],"   /      \\   ");
    strcpy(R[12].art[4],"  /________\\  ");

    strcpy(R[13].name,"The Void Gate");
    strcpy(R[13].desc,"Reality tears open. A swirling portal of darkness. A dying warrior lies at its base.");
    R[13].n=14; R[13].s=12; R[13].e=-1; R[13].w=-1;
    R[13].enemy_id=-1; R[13].npc_id=5; R[13].zone=3;
    R[13].art_lines=5;
    strcpy(R[13].art[0],"  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ");
    strcpy(R[13].art[1],"  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ");
    strcpy(R[13].art[2],"  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ");
    strcpy(R[13].art[3],"  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ");
    strcpy(R[13].art[4],"  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ~~~  ");

    strcpy(R[14].name,"The Void Wastes");
    strcpy(R[14].desc,"Nothingness stretches in all directions. Void Sentinels patrol. The Oracle whispers.");
    R[14].n=15; R[14].s=13; R[14].e=-1; R[14].w=-1;
    R[14].enemy_id=11; R[14].npc_id=4; R[14].zone=3;
    R[14].art_lines=5;
    strcpy(R[14].art[0],"  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
    strcpy(R[14].art[1],"  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
    strcpy(R[14].art[2],"  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
    strcpy(R[14].art[3],"  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
    strcpy(R[14].art[4],"  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");

    strcpy(R[15].name,"The Dragon's Den");
    strcpy(R[15].desc,"A colossal dragon guards the passage. Its scales shimmer with void energy.");
    R[15].n=16; R[15].s=14; R[15].e=-1; R[15].w=-1;
    R[15].enemy_id=10; R[15].npc_id=-1; R[15].zone=3;
    R[15].art_lines=5;
    strcpy(R[15].art[0],"        /\\  ");
    strcpy(R[15].art[1],"       /  \\ ");
    strcpy(R[15].art[2],"  /\\  / ** \\");
    strcpy(R[15].art[3]," /  \\/  ~~  ");
    strcpy(R[15].art[4],"/    \\_____/");

    strcpy(R[16].name,"The Bone Fields");
    strcpy(R[16].desc,"Fields of bones stretch endlessly. A Bone Colossus rises from the remains.");
    R[16].n=17; R[16].s=15; R[16].e=-1; R[16].w=-1;
    R[16].enemy_id=15; R[16].npc_id=-1; R[16].zone=3;
    R[16].art_lines=5;
    strcpy(R[16].art[0],"  _  _  _  _  _  _  _  _  _  _  _  _  _  _  ");
    strcpy(R[16].art[1]," | || || || || || || || || || || || || || | ");
    strcpy(R[16].art[2]," |_||_||_||_||_||_||_||_||_||_||_||_||_||_| ");
    strcpy(R[16].art[3],"  _  _  _  _  _  _  _  _  _  _  _  _  _  _  ");
    strcpy(R[16].art[4]," | || || || || || || || || || || || || || | ");

    strcpy(R[17].name,"The Guardian's Chamber");
    strcpy(R[17].desc,"An ancient construct blocks the way. It was built to protect... or imprison.");
    R[17].n=18; R[17].s=16; R[17].e=-1; R[17].w=-1;
    R[17].enemy_id=13; R[17].npc_id=-1; R[17].zone=3;
    R[17].art_lines=5;
    strcpy(R[17].art[0],"   /====\\   ");
    strcpy(R[17].art[1],"  |  @@  |  ");
    strcpy(R[17].art[2],"  |      |  ");
    strcpy(R[17].art[3]," /|      |\\ ");
    strcpy(R[17].art[4]," \\|      |/ ");

    strcpy(R[18].name,"The Healing Spring");
    strcpy(R[18].desc,"A luminous spring of pure crystal water. Its glow pushes back the Void's darkness.");
    R[18].n=19; R[18].s=17; R[18].e=-1; R[18].w=-1;
    R[18].enemy_id=-1; R[18].npc_id=-1; R[18].zone=3;
    R[18].is_heal=1;
    R[18].art_lines=6;
    strcpy(R[18].art[0],"    ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~    ");
    strcpy(R[18].art[1],"  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ");
    strcpy(R[18].art[2],"    ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~    ");
    strcpy(R[18].art[3],"  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ");
    strcpy(R[18].art[4],"    ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~    ");
    strcpy(R[18].art[5],"  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ");

    strcpy(R[19].name,"The Void King's Throne");
    strcpy(R[19].desc,"THE FINAL BATTLE. The Void King rises from his throne of shadows!");
    R[19].n=-1; R[19].s=18; R[19].e=-1; R[19].w=-1;
    R[19].enemy_id=9; R[19].npc_id=-1; R[19].zone=3;
    R[19].art_lines=7;
    strcpy(R[19].art[0],"  \\\\  ||  //  ");
    strcpy(R[19].art[1],"   \\\\ || //   ");
    strcpy(R[19].art[2],"    \\\\||//    ");
    strcpy(R[19].art[3],"   /|    |\\   ");
    strcpy(R[19].art[4],"  | |XX  | |  ");
    strcpy(R[19].art[5],"  | |    | |  ");
    strcpy(R[19].art[6]," /| |    | |\\ ");

    strcpy(R[20].name,"The Restored Realm");
    strcpy(R[20].desc,"Light floods the world. The Crystal reforms. The realm is whole again!");
    R[20].n=-1; R[20].s=-1; R[20].e=-1; R[20].w=-1;
    R[20].enemy_id=-1; R[20].npc_id=-1; R[20].zone=3;
    R[20].art_lines=6;
    strcpy(R[20].art[0],"    *  .  *  .  *  .  *  .  *  .  *  .  *  ");
    strcpy(R[20].art[1],"  .  *  .  *  .  *  .  *  .  *  .  *  .  *  ");
    strcpy(R[20].art[2],"    *  .  *  .  *  .  *  .  *  .  *  .  *  ");
    strcpy(R[20].art[3],"  .  *  .  *  .  *  .  *  .  *  .  *  .  *  ");
    strcpy(R[20].art[4],"    *  .  *  .  *  .  *  .  *  .  *  .  *  ");
    strcpy(R[20].art[5],"  .  *  .  *  .  *  .  *  .  *  .  *  .  *  ");
}

void init_player() {
    memset(&P, 0, sizeof(P));
    strcpy(P.name,"Wanderer");
    P.atk=10; P.def=5; P.max_hp=100; P.hp=100;
    P.xp=0; P.level=1; P.gold=50;
    P.inv_count=0;
    P.crit_chance=8; P.dodge_chance=5;
    P.special_ready=0;
}

void add_item(const char *name) {
    if (P.inv_count < MAX_INV) {
        strcpy(P.inv[P.inv_count], name);
        P.inv_count++;
        printf(BGRN "  ✦ Acquired: %s" RST "\n", name);
    }
}

void check_levelup() {
    int needed = xp_needed(P.level);
    while (P.xp >= needed && P.level < 30) {
        P.level++;
        P.max_hp += 12;
        P.hp = P.max_hp;
        P.atk += 2;
        P.def += 1;
        if (P.level % 3 == 0) P.crit_chance += 1;
        if (P.level % 5 == 0) P.dodge_chance += 1;
        printf(BYEL "\n  ═══ LEVEL UP! Level %d! HP:%d ATK:%d DEF:%d ═══" RST "\n",
               P.level, P.max_hp, P.atk, P.def);
        needed = xp_needed(P.level);
    }
}

void show_minimap() {
    printf("  " DIM "MAP:" RST " ");
    for (int i = 0; i < 20; i++) {
        if (i == cur_room) printf(BYEL "◉" RST);
        else if (rooms_visited[i]) printf(BGRN "●" RST);
        else printf(DIM "○" RST);
        if (i == 4 || i == 8 || i == 12 || i == 16) printf(" ");
    }
    printf("\n");
}

void show_status() {
    printf("\n" BBLU "  ╔══════════════════════════════════════════════════════════════╗" RST "\n");
    printf(BBLU "  ║" RST "  " BOLD "%-12s" RST "  Lv:%-3d  Gold:%-5d  XP:%-5d/%-5d  " BBLU "║" RST "\n",
        P.name, P.level, P.gold, P.xp, xp_needed(P.level));
    printf(BBLU "  ║" RST "  HP: ");
    hp_bar(P.hp, P.max_hp, 28);
    printf("\n");
    printf(BBLU "  ║" RST "  ATK:%-4d  DEF:%-4d  Crit:%2d%%  Dodge:%2d%%  " BBLU "║" RST "\n",
        P.atk, P.def, P.crit_chance, P.dodge_chance);
    printf(BBLU "  ║" RST "  Sword:%s Shield:%s Amulet:%s Key:%s  " BBLU "║" RST "\n",
        P.has_sword?"YES":"no ", P.has_shield?"YES":"no ", P.has_amulet?"YES":"no ", P.has_key?"YES":"no ");
    printf(BBLU "  ║" RST "  Crystal:%s Crown:%s DragonHeart:%s  " BBLU "║" RST "\n",
        P.has_crystal?"YES":"no ", P.has_crown?"YES":"no ", P.has_dragon_heart?"YES":"no ");
    printf(BBLU "  ╚══════════════════════════════════════════════════════════════╝" RST "\n");
    show_minimap();
}

void show_room() {
    Room *r = &R[cur_room];
    if (!rooms_visited[cur_room]) { rooms_visited[cur_room] = 1; explored_count++; }
    zone_transition(r->zone);
    clr();
    sep2();
    printf(BOLD "  %s" RST "\n", r->name);
    sep2();
    printf("\n");
    for (int i = 0; i < r->art_lines; i++) {
        if (r->zone == 0) printf(GRN);
        else if (r->zone == 1) printf(CYN);
        else if (r->zone == 2) printf(MAG);
        else printf(BRED);
        printf("  %s\n", r->art[i]);
    }
    printf(RST "\n");
    printf("  %s\n\n", r->desc);
    sep();
    printf("  Exits:");
    if (r->n >= 0) printf(" " BOLD "N" RST);
    if (r->s >= 0) printf(" " BOLD "S" RST);
    if (r->e >= 0) printf(" " BOLD "E" RST);
    if (r->w >= 0) printf(" " BOLD "W" RST);
    if (r->enemy_id >= 0 && E[r->enemy_id].hp > 0) printf("  " BRED "[⚔ ENEMY!]" RST);
    if (r->npc_id >= 0) printf("  " BYEL "[💬 NPC]" RST);
    if (r->has_item) printf("  " BGRN "[✦ ITEM]" RST);
    if (r->is_heal) printf("  " BCYN "[✧ HEAL]" RST);
    printf("\n");
    show_minimap();
    sep();
    printf("  " BOLD "N/S/E/W" RST " move | " BOLD "A" RST " attack | " BOLD "T" RST " talk | " BOLD "I" RST " inv | " BOLD "U" RST " use | " BOLD "H" RST " status | " BOLD "R" RST " rest | " BOLD "Q" RST " quit\n");
    sep();
}

void boss_intro(Enemy *en) {
    clr();
    printf("\n\n\n\n\n");
    sep3();
    printf("\n");
    /* Dramatic typing of boss name */
    printf("  ");
    for (int i = 0; en->name[i]; i++) {
        printf(BRED "%c" RST, en->name[i]);
        fflush(stdout);
        usleep(80000);
    }
    printf("\n\n");
    printf(BRED "  has awakened!" RST "\n\n");
    usleep(300000);
    printf("  ");
    for (int i = 0; i < en->art_lines; i++) {
        printf("%s%s" RST "\n", en->color, en->art[i]);
        usleep(100000);
    }
    printf("\n");
    usleep(500000);
}

void do_combat() {
    Enemy *en = &E[cur_enemy];
    in_combat = 1;
    int round = 0;

    if (en->is_boss) {
        boss_intro(en);
    } else {
        printf("\n" BRED "  ⚔ COMBAT: %s ⚔" RST "\n\n", en->name);
    }

    for (int i = 0; i < en->art_lines; i++)
        printf("  %s%s" RST "\n", en->color, en->art[i]);
    printf("\n");

    while (in_combat && !game_over) {
        round++;
        if (time(NULL) - start_time > 750) { game_over = 1; break; }

        printf("  %s HP: ", en->name);
        hp_bar(en->hp, en->max_hp, 25);
        printf("\n  Your HP: ");
        hp_bar(P.hp, P.max_hp, 25);
        printf("\n  [A]ttack [S]pecial [P]otion [F]lee? ");
        fflush(stdout);

        char ch;
        if (interactive_mode) {
            ch = get_key();
            if (ch == '\0') ch = 'a';
        } else {
            usleep(80000);
            int has_potion = 0;
            for (int i = 0; i < P.inv_count; i++)
                if (strstr(P.inv[i], "Potion")) { has_potion = 1; break; }
            if (P.hp < P.max_hp / 2 && has_potion) ch = 'p';
            else if (P.special_ready && round % 4 == 0) ch = 's';
            else ch = 'a';
            printf("%c\n", ch);
        }

        if (ch == 'a' || ch == 'A') {
            int dmg = P.atk + rand() % 6 - en->def;
            if (P.has_sword) dmg += 4;
            if (P.has_crown) dmg += 6;
            if (P.has_crystal) dmg += 3;
            if (P.has_dragon_heart) dmg += 4;
            int is_crit = (rand() % 100) < P.crit_chance;
            if (is_crit) { dmg = dmg * 2 + 3; crit_count++; }
            if (dmg < 1) dmg = 1;
            en->hp -= dmg;
            total_damage_dealt += dmg;
            if (is_crit) printf(BYEL "  ★ CRITICAL HIT! %d damage!" RST "\n", dmg);
            else printf(BGRN "  You strike for %d damage!" RST "\n", dmg);

            if (en->hp <= 0) {
                printf(BYEL "\n  ✦ %s defeated! +%d XP, +%d Gold" RST "\n", en->name, en->xp_r, en->gold_r);
                P.xp += en->xp_r; P.gold += en->gold_r; enemies_defeated++;
                if (en->drops) {
                    printf(BGRN "  Dropped: %s" RST "\n", en->drop_name);
                    add_item(en->drop_name);
                    if (strcmp(en->drop_name, "Guardian's Key") == 0) P.has_key = 1;
                    if (strcmp(en->drop_name, "Dragon Heart") == 0) { P.has_dragon_heart = 1; P.atk += 4; P.max_hp += 15; P.hp += 15; }
                }
                if (rand() % 3 == 0) add_item("Healing Potion");
                check_levelup();
                in_combat = 0;
                if (cur_enemy == 9) { game_won = 1; game_over = 1; }
                break;
            }
            if (en->is_boss && en->phase == 0 && en->hp < en->max_hp * 2 / 3) {
                en->phase = 1; en->atk += 3;
                printf(BRED "\n  ⚡ %s enters PHASE 2! Attack increased!" RST "\n", en->name);
                usleep(200000);
            }
            if (en->is_boss && en->phase == 1 && en->hp < en->max_hp / 3) {
                en->phase = 2; en->atk += 3; en->def += 2;
                printf(BRED "\n  ⚡⚡ %s enters PHASE 3! ENRAGED!" RST "\n", en->name);
                usleep(200000);
            }
        } else if (ch == 's' || ch == 'S') {
            if (!P.special_ready) { printf(DIM "  Special not ready!\n" RST); continue; }
            int dmg = P.atk * 2 + 8;
            if (P.has_crown) dmg += 10;
            if (P.has_crystal) dmg += 6;
            en->hp -= dmg;
            total_damage_dealt += dmg;
            P.special_ready = 0;
            printf(BMAG "  ✧ SPECIAL ATTACK! %d damage!" RST "\n", dmg);
            if (en->hp <= 0) {
                printf(BYEL "\n  ✦ %s defeated! +%d XP, +%d Gold" RST "\n", en->name, en->xp_r, en->gold_r);
                P.xp += en->xp_r; P.gold += en->gold_r; enemies_defeated++;
                if (en->drops) { add_item(en->drop_name); if (strcmp(en->drop_name,"Guardian's Key")==0) P.has_key=1; if (strcmp(en->drop_name,"Dragon Heart")==0){P.has_dragon_heart=1;P.atk+=4;P.max_hp+=15;P.hp+=15;} }
                if (rand() % 3 == 0) add_item("Healing Potion");
                check_levelup(); in_combat = 0;
                if (cur_enemy == 9) { game_won = 1; game_over = 1; }
                break;
            }
        } else if (ch == 'p' || ch == 'P') {
            int found = 0;
            for (int i = 0; i < P.inv_count; i++) {
                if (strstr(P.inv[i], "Potion")) {
                    int heal = 35 + P.level * 3;
                    P.hp += heal;
                    if (P.hp > P.max_hp) P.hp = P.max_hp;
                    printf(BGRN "  You drink a potion. +%d HP!" RST "\n", heal);
                    for (int j = i; j < P.inv_count - 1; j++) strcpy(P.inv[j], P.inv[j+1]);
                    P.inv_count--;
                    found = 1; break;
                }
            }
            if (!found) { printf(BRED "  No potions!\n" RST); continue; }
        } else if (ch == 'f' || ch == 'F') {
            if (en->is_boss) { printf(BRED "  Cannot flee from a boss!\n" RST); continue; }
            else { printf(YEL "  You flee!\n" RST); in_combat = 0; break; }
        }

        if (in_combat && en->hp > 0) {
            if ((rand() % 100) < P.dodge_chance) {
                dodge_count++;
                printf(BGRN "  You dodge the attack!" RST "\n");
            } else {
                int edmg = en->atk + rand() % 5 - P.def;
                if (P.has_shield) edmg -= 3;
                if (P.has_amulet) edmg -= 2;
                if (edmg < 1) edmg = 1;
                if (en->is_boss && en->special_round > 0 && round % en->special_round == 0) {
                    edmg = edmg * 2 + 3;
                    printf(BRED "  ⚡ %s uses %s! %d damage!" RST "\n", en->name, en->special_msg, edmg);
                } else {
                    printf(BRED "  %s hits you for %d damage!" RST "\n", en->name, edmg);
                }
                P.hp -= edmg;
                total_damage_taken += edmg;
                if (P.hp <= 0) {
                    printf("\n" BRED "  ═══ YOU HAVE FALLEN ═══" RST "\n");
                    game_over = 1; break;
                }
            }
        }

        if (round % 4 == 0 && !P.special_ready) {
            P.special_ready = 1;
            printf(DIM "  [Special attack charged!]" RST "\n");
        }
    }
}

void do_npc() {
    NPC *npc = &N[R[cur_room].npc_id];
    clr(); sep2();
    printf("  %s%s" RST "\n\n", npc->color, npc->name);
    sep();
    for (int i = 0; i < npc->count; i++) type_slow(npc->lines[i]);
    sep();
    if (npc->gives) {
        printf("\n  %s gives you: " BGRN "%s" RST "\n", npc->name, npc->give_name);
        add_item(npc->give_name);
        if (strcmp(npc->give_name, "Crystal Amulet") == 0) { P.has_amulet = 1; P.def += 3; }
        if (strcmp(npc->give_name, "Crown of Dawn") == 0) { P.has_crown = 1; P.atk += 6; P.def += 3; P.crit_chance += 4; }
        if (strcmp(npc->give_name, "Void Key") == 0) P.has_key = 1;
        npc->gives = 0;
    }
    printf("\n  Press Enter..."); fflush(stdout); wait_enter();
}

void do_inventory() {
    clr(); sep2(); printf("  INVENTORY\n"); sep2();
    if (P.inv_count == 0) printf("  (empty)\n");
    for (int i = 0; i < P.inv_count; i++) printf("  %d. %s\n", i+1, P.inv[i]);
    sep(); printf("  Press Enter..."); fflush(stdout); wait_enter();
}

void do_use() {
    printf("  Use which item? (number): "); fflush(stdout);
    int n;
    if (interactive_mode) { if (scanf("%d", &n) != 1) { getchar(); return; } getchar(); }
    else { n = 1; printf("1\n"); }
    if (n < 1 || n > P.inv_count) { printf("  Invalid.\n"); return; }
    if (strstr(P.inv[n-1], "Potion")) {
        int heal = 35 + P.level * 3; P.hp += heal; if (P.hp > P.max_hp) P.hp = P.max_hp;
        printf(BGRN "  +%d HP!" RST "\n", heal);
        for (int j = n-1; j < P.inv_count-1; j++) strcpy(P.inv[j], P.inv[j+1]); P.inv_count--;
    } else if (strstr(P.inv[n-1], "Sword")) { P.has_sword=1; P.atk+=4; printf(BGRN "  Equipped! +4 ATK\n" RST); }
    else if (strstr(P.inv[n-1], "Shield")) { P.has_shield=1; P.def+=4; printf(BGRN "  Equipped! +4 DEF\n" RST); }
    else if (strstr(P.inv[n-1], "Amulet")) { P.has_amulet=1; P.def+=3; printf(BGRN "  Equipped! +3 DEF\n" RST); }
    else if (strstr(P.inv[n-1], "Crown")) { P.has_crown=1; P.atk+=6; P.def+=3; P.crit_chance+=4; printf(BYEL "  Crown! +6 ATK, +3 DEF\n" RST); }
    else if (strstr(P.inv[n-1], "Crystal")) { P.has_crystal=1; P.atk+=3; printf(BCYN "  Crystal! +3 ATK\n" RST); }
    else if (strstr(P.inv[n-1], "Dragon Heart")) { P.has_dragon_heart=1; P.atk+=4; P.max_hp+=15; P.hp+=15; printf(BRED "  Dragon Heart! +4 ATK, +15 HP\n" RST); }
    else printf("  Cannot use that.\n");
    printf("  Press Enter..."); fflush(stdout); wait_enter();
}

void do_shop() {
    clr(); sep2(); printf("  " BYEL "MERCHANT'S WARES" RST "\n"); sep2();
    printf("  1. Iron Sword       - 30 gold  (+4 ATK)\n");
    printf("  2. Wooden Shield    - 25 gold  (+4 DEF)\n");
    printf("  3. Healing Potion   - 10 gold  (+35 HP)\n");
    printf("  4. Health Potion x3 - 25 gold  (3 potions)\n");
    printf("  0. Leave\n"); sep();
    printf("  Your gold: %d\n  Choice: ", P.gold); fflush(stdout);
    int c;
    if (interactive_mode) { if (scanf("%d", &c) != 1) { getchar(); return; } getchar(); }
    else { if (!P.has_sword) c = 1; else c = 4; printf("%d\n", c); }
    if (c == 1 && P.gold >= 30) { P.gold -= 30; add_item("Iron Sword"); P.has_sword = 1; P.atk += 4; }
    else if (c == 2 && P.gold >= 25) { P.gold -= 25; add_item("Wooden Shield"); P.has_shield = 1; P.def += 4; }
    else if (c == 3 && P.gold >= 10) { P.gold -= 10; add_item("Healing Potion"); }
    else if (c == 4 && P.gold >= 25) { P.gold -= 25; add_item("Healing Potion"); add_item("Healing Potion"); add_item("Healing Potion"); }
    else if (c != 0) printf("  Not enough gold!\n");
    printf("  Press Enter..."); fflush(stdout); wait_enter();
}

void do_rest() {
    if (P.rested[cur_room]) { printf("  You already rested here.\n"); return; }
    int heal = P.max_hp / 5;
    P.hp += heal;
    if (P.hp > P.max_hp) P.hp = P.max_hp;
    P.rested[cur_room] = 1;
    printf(BGRN "  You rest and recover %d HP." RST "\n", heal);
}

void intro() {
    loading_screen();
    clr();
    printf("\n");
    printf(BRED "  ███████╗██╗  ██╗ █████╗ ██████╗ ███████╗███████╗██████╗ " RST "\n");
    printf(BRED "  ██╔════╝██║  ██║██╔══██╗██╔══██╗██╔════╝██╔════╝██╔══██╗" RST "\n");
    printf(BRED "  ███████╗███████║███████║██║  ██║█████╗  █████╗  ██████╔╝" RST "\n");
    printf(BRED "  ╚════██║██╔══██║██╔══██║██║  ██║██╔══╝  ██╔══╝  ██╔══██╗" RST "\n");
    printf(BRED "  ███████║██║  ██║██║  ██║██████╔╝███████╗███████╗██║  ██║" RST "\n");
    printf(BRED "  ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝" RST "\n");
    printf("\n");
    printf(BYEL "         THE SHATTERED REALM" RST "\n");
    printf(BWHT "     A Terminal Adventure" RST "\n\n");
    sep();
    type_slow("  The world was once whole, united by the Crystal of Unity.");
    type_slow("  But the Void King shattered it, splitting reality into fragments.");
    type_slow("  Forests, caves, castles, and the Void itself now drift apart.");
    type_slow("  You are the last hope. Journey through the shattered lands,");
    type_slow("  grow stronger, and confront the Void King in his dark throne.");
    type_slow("");
    printf("  " BOLD "Controls:" RST " N/S/E/W move | A attack | S special | T talk\n");
    printf("  " BOLD "         " RST " I inventory | U use item | H status | R rest | Q quit\n");
    type_slow("");
    printf("  " BGRN "Good luck, Wanderer." RST "\n");
    sep();
    printf("\n  Press Enter to begin..."); fflush(stdout); wait_enter();
}

void ending() {
    clr();
    printf("\n\n");
    printf(BGRN "  ██████╗  ██████╗ ██████╗ ███████╗███████╗██████╗ " RST "\n");
    printf(BGRN "  ██╔══██╗██╔═══██╗██╔══██╗██╔════╝██╔════╝██╔══██╗" RST "\n");
    printf(BGRN "  ██████╔╝██║   ██║██║  ██║█████╗  █████╗  ██████╔╝" RST "\n");
    printf(BGRN "  ██╔══██╗██║   ██║██║  ██║██╔══╝  ██╔══╝  ██╔══██╗" RST "\n");
    printf(BGRN "  ██║  ██║╚██████╔╝██████╔╝███████╗███████╗██║  ██║" RST "\n");
    printf(BGRN "  ╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝" RST "\n");
    printf("\n");
    type_slow("  The Void King falls. Light floods the shattered realm.");
    type_slow("  The Crystal of Unity reforms, whole and radiant.");
    type_slow("  The world is restored. You are the hero!");
    printf("\n");
    /* Particle burst */
    const char *chars[] = {"✦","✧","★","☆","◆","◇","●","○","♦","♠","♣","♥"};
    for (int i = 0; i < 30; i++) {
        int x = 5 + rand() % 70;
        int y = 2 + rand() % 20;
        gotoxy(x, y);
        printf(BYEL "%s" RST, chars[rand() % 12]);
        fflush(stdout);
        usleep(20000);
    }
    printf("\n");
    sep2();
    printf("  " BOLD "FINAL STATS" RST "\n");
    sep();
    printf("  Level: %d  |  Gold: %d  |  XP: %d\n", P.level, P.gold, P.xp);
    printf("  Enemies Defeated: %d\n", enemies_defeated);
    printf("  Total Damage Dealt: %d\n", total_damage_dealt);
    printf("  Total Damage Taken: %d\n", total_damage_taken);
    printf("  Critical Hits: %d  |  Dodges: %d\n", crit_count, dodge_count);
    printf("  Rooms Explored: %d/20\n", explored_count);
    sep();
    printf("  Items: ");
    for (int i = 0; i < P.inv_count; i++) printf("%s ", P.inv[i]);
    printf("\n");
    sep2();
    printf("\n  " BYEL "Thank you for playing THE SHATTERED REALM!" RST "\n");
    printf("  " DIM "A terminal adventure in 20 rooms, 12 enemies, 3 bosses." RST "\n\n");
}

void game_over_screen() {
    clr();
    printf("\n\n");
    printf(BRED "  ██████╗  ██████╗ ██████╗ ███████╗███████╗██████╗ " RST "\n");
    printf(BRED "  ██╔══██╗██╔═══██╗██╔══██╗██╔════╝██╔════╝██╔══██╗" RST "\n");
    printf(BRED "  ██████╔╝██║   ██║██║  ██║█████╗  █████╗  ██████╔╝" RST "\n");
    printf(BRED "  ██╔══██╗██║   ██║██║  ██║██╔══╝  ██╔══╝  ██╔══██╗" RST "\n");
    printf(BRED "  ██║  ██║╚██████╔╝██████╔╝███████╗███████╗██║  ██║" RST "\n");
    printf(BRED "  ╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝" RST "\n");
    printf("\n");
    printf(BRED "  The darkness claims you. The realm remains shattered.\n" RST "\n");
    sep();
    printf("  You reached Level %d with %d Gold.\n", P.level, P.gold);
    printf("  Enemies Defeated: %d\n", enemies_defeated);
    printf("  Rooms Explored: %d/20\n", explored_count);
    sep();
    printf("\n  Press Enter to exit...\n"); fflush(stdout); wait_enter();
}

void run_demo() {
    int path[] = {0, 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    int path_len = 18;

    for (int step = 0; step < path_len && !game_over; step++) {
        if (time(NULL) - start_time > 700) break;
        cur_room = path[step];
        Room *r = &R[cur_room];
        show_room();
        usleep(300000);

        if (r->is_heal) {
            printf(BCYN "  ✧ The spring's glow washes over you. Fully restored!" RST "\n");
            P.hp = P.max_hp;
            usleep(200000);
        }

        if (r->npc_id >= 0) {
            if (r->npc_id == 1) do_shop();
            else do_npc();
        }

        if (r->has_item) {
            printf(BGRN "  You see: %s. Taking it!" RST "\n", r->item_name);
            add_item(r->item_name);
            if (strcmp(r->item_name, "Crystal Shard") == 0) { P.has_crystal = 1; P.atk += 3; }
            if (strcmp(r->item_name, "Torch") == 0) P.has_torch = 1;
            r->has_item = 0;
            usleep(150000);
        }

        if (r->enemy_id >= 0 && E[r->enemy_id].hp > 0) {
            cur_enemy = r->enemy_id;
            do_combat();
            if (game_over) break;
            if (game_won) break;
        }
        usleep(150000);
    }

    if (game_won) ending();
    else if (game_over) game_over_screen();
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    start_time = time(NULL);
    init_enemies();
    init_npcs();
    init_rooms();
    init_player();
    cur_room = 0;
    game_over = 0; in_combat = 0; game_won = 0;

    if (argc > 1 && strcmp(argv[1], "--interactive") == 0) interactive_mode = 1;
    else if (argc > 1 && strcmp(argv[1], "--demo") == 0) interactive_mode = 0;
    else {
        struct pollfd pfd; pfd.fd = STDIN_FILENO; pfd.events = POLLIN;
        int ret = poll(&pfd, 1, 50);
        if (ret > 0 && isatty(STDIN_FILENO)) interactive_mode = 1;
    }

    if (!interactive_mode) { intro(); run_demo(); return 0; }

    intro();

    while (!game_over) {
        if (time(NULL) - start_time > 780) { printf("\n" YEL "  Time's up!\n" RST); break; }
        show_room();
        Room *r = &R[cur_room];

        if (r->is_heal && !P.rested[cur_room]) {
            printf(BCYN "  ✧ The spring's glow washes over you. Fully restored!" RST "\n");
            P.hp = P.max_hp;
            P.rested[cur_room] = 1;
            usleep(200000);
        }

        if (r->enemy_id >= 0 && E[r->enemy_id].hp > 0) {
            cur_enemy = r->enemy_id;
            do_combat();
            if (game_over) break;
            if (game_won) break;
            continue;
        }

        printf("\n  > "); fflush(stdout);
        char cmd = get_key();
        if (cmd == '\0') continue;

        if (cmd == 'q' || cmd == 'Q') { game_over = 1; break; }
        else if (cmd == 'n' || cmd == 'N') { if (r->n >= 0) cur_room = r->n; else printf("  Can't go that way.\n"); }
        else if (cmd == 's' || cmd == 'S') { if (r->s >= 0) cur_room = r->s; else printf("  Can't go that way.\n"); }
        else if (cmd == 'e' || cmd == 'E') { if (r->e >= 0) cur_room = r->e; else printf("  Can't go that way.\n"); }
        else if (cmd == 'w' || cmd == 'W') { if (r->w >= 0) cur_room = r->w; else printf("  Can't go that way.\n"); }
        else if (cmd == 'h' || cmd == 'H') { show_status(); printf("  Press Enter..."); fflush(stdout); wait_enter(); }
        else if (cmd == 'r' || cmd == 'R') { do_rest(); }
        else if (cmd == 't' || cmd == 'T') {
            if (r->npc_id >= 0) { if (r->npc_id == 1) do_shop(); else do_npc(); }
            else printf("  No one here.\n");
        }
        else if (cmd == 'a' || cmd == 'A') {
            if (r->enemy_id >= 0 && E[r->enemy_id].hp > 0) { cur_enemy = r->enemy_id; do_combat(); }
            else printf("  Nothing to attack.\n");
        }
        else if (cmd == 'i' || cmd == 'I') do_inventory();
        else if (cmd == 'u' || cmd == 'U') do_use();

        if (game_won) { ending(); break; }

        if (r->has_item) {
            printf(BGRN "  You see: %s. Take it? (y/n): " RST, r->item_name); fflush(stdout);
            char y = get_key();
            if (y == 'y' || y == 'Y') {
                add_item(r->item_name);
                if (strcmp(r->item_name, "Crystal Shard") == 0) { P.has_crystal = 1; P.atk += 3; }
                if (strcmp(r->item_name, "Torch") == 0) P.has_torch = 1;
                r->has_item = 0;
            }
        }

        if (cur_room == 19 && !P.has_crown) {
            printf(BRED "  The Void King's power is too great without the Crown!\n" RST);
            cur_room = 18;
        }
    }

    if (game_over && !game_won) game_over_screen();
    return 0;
}