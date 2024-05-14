#include "keyboard_map.h"

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2

#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES


#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08


#define ENTER_KEY_CODE 0x1C
#define UP_KEY_CODE 0x48
#define DOWN_KEY_CODE 0x50
#define SPACE_KEY_CODE 0x39
#define Q_KEY_CODE 0x10
#define NULL_CHARACTER_CODE 0x00

#define SIZE 15
int board[SIZE-5][SIZE];



extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

void kb_init(void);
void kprint(const char *str,int color);
void kprint_newline(void);
void clear_screen(void);
void sleep(int sec);
void gotoxy(unsigned int x, unsigned int y);
void draw_strxy(const char *str,unsigned int x, unsigned int y);
void end_screen(void);


void Draw();                                                                // to draw and show the current state of the game on the screen
void Start();                                                               // to set the game state at startup (like determining the starting position of the player or enemy)
void HitTest();                                                             // to control collisions (such as a bullet hitting an enemy)
void SpawnEnemy();                                                          // to spawn an enemy
void SpawnBullet();                                                         // to spawn a bullet
void DespawnBullet(int idx);                                                // to destroy the bullet if it hits the target or leaves the screen
int playerCollision();                                                             // to control the situation where the enemy touches the player
int IsBullet(int x, int y);  
int simple_rand();



unsigned int current_loc = 0;         // current cursor location


char *vidptr = (char*)0xb8000;        // video memory begins at address 0xb8000



struct IDT_entry {
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};


struct IDT_entry IDT[IDT_SIZE];

// Initializes the Interrupt Descriptor Table (IDT)
void idt_init(void) {

	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;


// Programmable Interrupt Controller (PIC) initialization
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);

	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);

	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}



int bulletCount = 0;                                                        // Holds the total number of bullets in the game. Initially, it is initialized to 0 because there are no bullets in the game.
int FIRE = 0;                                                               // Holds the number of frames since the last fire. This is used to control when the next fire is fired.
int playerY = 8;                                                            // Player's Y position
int FIRERATE = 500;                                                         // It determines how many milliseconds a player can fire a bullet.
int SPEED = 75;                                                             // It determines the delay between frames, i.e. how fast the game runs. A lower value means a faster game.


typedef struct {                                                            // The word "struct" allows you to define your own data type by combining variables of one or more different data types.
    int x;                                                                  // Enemy's X position
    int y;                                                                  // Enemy's Y position
} PIX;                                                                      // Our struct name is "PIX".

PIX bullets[100];                                                           // An array for the bullets.
PIX enemy;


// Initializes the keyboard
void kb_init(void) {
	write_port(0x21 , 0xFD);
}

// Prints a string with the specified color
void kprint(const char *str,int color) {

	unsigned int i = 0;

	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = color;
	}

}

// Moves to the next line
void kprint_newline(void) {
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}


// Clears the screen
void clear_screen(void) {
	unsigned int i = 0;

	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
	}

	current_loc = 0;
}


// Delays for a given number of seconds
void sleep(int sec) {
	int i;

  for(i = 0; i < sec; i++);
}


// Moves cursor to specified location on the screen
void gotoxy(unsigned int x, unsigned int y) {

	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = BYTES_FOR_EACH_ELEMENT * (x * COLUMNS_IN_LINE + y);

}


// Draws a string at the specified location on the screen
void draw_strxy(const char *str,unsigned int x, unsigned int y) {
	gotoxy(y,x);
	kprint(str,0x0F);
}



// Main keyboard handler function
void keyboard_handler_main(void) {

    enemy.x--;                                                              // moves the enemy to the left by subtracting 1 from the enemy's x position and then,
    HitTest();                                                              // the HitTest function is called to check the collision.

    for (int i = 0; i < bulletCount; i++) {                                 // checks the movement of the available bullets and checks if the they reach the right edge of the screen

        if (bullets[i].x >= 77) {                                           // If bullet x is greater then or equal to 77 (77 is the range we set as the far right of the screen),
            DespawnBullet(i);                                               // then despawn the bullet.
        }

        bullets[i].x += 2;                                                  // If the bullet has not reached the right edge of the screen,
                                                                            // the bullet is moved 2 units to the right (you can think of 2 units as bullet speed).

    }

  unsigned char status;
	char keycode;


	  write_port(0x20, 0x20);

	  status = read_port(KEYBOARD_STATUS_PORT);

    if (status & 0x01) {

      keycode = read_port(KEYBOARD_DATA_PORT);

      if(keycode < 0) {
			  return;
      }


      // Switch-Case for movement of the Player and Fire.
      switch (keycode) {


          case UP_KEY_CODE:
              if (playerY > 1) {                                              // 1 is the vertex point.
                  playerY--;                                                  // Moves up 1 unit.
              }
              break;



          case DOWN_KEY_CODE:
              if (playerY < 15) {                                             // 15 is the bottom point.
                  playerY++;                                                  // Moves down 1 unit.
              }
              break;



          case SPACE_KEY_CODE:                                                           // SPACE bar
              if (FIRE == 0) {
                  SpawnBullet();
              }
              break;



          case Q_KEY_CODE:
              while (1) {
                clear_screen();
              }                                                        // Exit game
              break;        

      }

    }
}



// Draws the game screen
void Draw() {

  for (int y = 0; y < 19; y++) { 

    for (int x = 0; x < 77; x++) {


      if (y == 0 || y == 18) { 
        const char *lines = "__";
        draw_strxy(lines,x,y);
      }


      else if (y == playerY && (x == 2 || x == 3)) { 

        if (x == 2) {
          const char *s1 = " /";
          draw_strxy(s1,x,y);
        }

        else if (x == 3) {
          const char *s2 = "]\\";
          draw_strxy(s2,x,y);
        }

      }

      else if (y == playerY + 1  &&  x >= 2  &&  x <= 5) {

        switch (x) {

          case 2:
            const char *s3 = "<";
            draw_strxy(s3,x,y);
            break;

          case 3:
            const char *s4 = "(";
            draw_strxy(s4,x,y);
            break;

          case 4:
            const char *s5 = "]";
            draw_strxy(s5,x,y);
            break;

          case 5:
            const char *s6 = ">";
            draw_strxy(s6,x,y);
            break;
        }

      }



      else if (y == playerY + 2  &&  (x == 2 || x == 3)) {

        if (x == 2) {
          const char *s7 = " \\";
          draw_strxy(s7,x,y);
        }

        else if (x == 3) {
          const char *s8 = "]/";
          draw_strxy(s8,x,y);
        }
        
      }



      else if (IsBullet(x, y)) {
        const char *b1 = "=";                                     // Bullet firing.
        draw_strxy(b1,x,y);
      }



      else if (x >= enemy.x  &&  x <= enemy.x + 3  &&  y == enemy.y) {

        if (x == enemy.x) {
          const char *e1 = " <";
          draw_strxy(e1,x,y);
        }

        else if (x == enemy.x + 1) {
          const char *e2 = "( ";
          draw_strxy(e2,x,y);
        }
                                                                            // Modeling the enemy space shuttle (to draw the main body part).
        else if (x == enemy.x + 2) {
          const char *e3 = "[>";
          draw_strxy(e3,x,y);
        }

        else if (x == enemy.x + 3) {
          const char *e4 = "C>";
          draw_strxy(e4,x,y);
        }

      }



      else if (( x == enemy.x + 1  ||  x == enemy.x + 2)  &&  y == enemy.y - 1) {

        if (x == enemy.x + 1) {
            const char *e5 = " /";
            draw_strxy(e5,x,y);
        }
                                                                            // Modeling the enemy space shuttle (to draw the upper body part).
        else if (x == enemy.x + 2) {
          const char *e6 = "-|";
          draw_strxy(e6,x,y);
        }

      }



      else if ((x == enemy.x + 1  ||  x == enemy.x + 2)  &&  y == enemy.y + 1) {

        if (x == enemy.x + 1) {
          const char *e7 = " \\";
          draw_strxy(e7,x,y);
        }
                                                                            // Modeling the enemy space shuttle (to draw the lower body part).
        else if (x == enemy.x + 2) {
          const char *e8 = "-|";
          draw_strxy(e8,x,y);
        }

      }



      else {
        const char *e9 = "  ";                                      // Adds a space character where no condition is met.
        draw_strxy(e9,x,y);
      }


    } // End of inner for loop.

  kprint_newline();                                                 // Moves to the next line.

  } // End of outer for loop.

}



// Initializes the game
void Start() {                                                              // Setup for the game

    for (int i = 0; i < 20; i++) {                                          // This is used to clear the top of the console screen before the game starts.

        clear_screen();
    }

    SpawnEnemy();                                                           // Spawn a enemy

}



// Performs hit testing for collisions
void HitTest() {

    if (enemy.x <= 1) {                                                    // All enemies have been destroyed and there are no more enemies.
      end_screen();                                                       //  The game ends.
    }

    if (playerCollision()) {                                                       // The player has failed to destroy the enemy and the enemy has entered the player's area.
      end_screen();                                                            // The player has lost and the game ends.
    }

    for (int i = 0; i < bulletCount; i++) {                                 // It tests whether each bullet has collided with the enemy.

        if (bullets[i].y >= enemy.y - 1   &&   bullets[i].y <= enemy.y + 1   &&   bullets[i].x >= enemy.x   &&   bullets[i].x <= enemy.x + 2) {
            // The bullet hits the enemy if these conditions are met.

            SpawnEnemy();                                                   // If a bullet hits an enemy, a new enemy is created.

            DespawnBullet(i);                                               // If a bullet hits the enemy, it destroys the bullet.

        }
    }
}


// Generates a simple random number within the given range
int simple_rand(int min, int max) {
    static unsigned int seed = 0xDEADBEEF;
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return min + (seed % (max - min + 1));
}


// Spawns an enemy
void SpawnEnemy() {

    enemy.y = simple_rand(5, 15);

    enemy.x = 77;     

}



// Spawns a bullet
void SpawnBullet() {

    FIRE = FIRERATE / SPEED;                                                // FIRERATE represents the time in milliseconds per shot, while SPEED represents the speed of the game.
                                                                            // Thus, the fire variable determines the number of frames between shots.

    bullets[bulletCount].x = 6;                                             // I set a specific point, 6, where the bullet will fire in front of the player's character.
    bullets[bulletCount].y = playerY + 1;                                   // Adds 1 unit to the player's line (playerY), placing the bullet directly above the player's character.
    bulletCount++;                                                          // Ensures that each time a new bullet is created, it will be written to the next bullet index.
}



// Despawns a bullet at the specified index
void DespawnBullet(int idx) {                                               // The value "idx" is used to determine which bullet is to be removed.

    for (int i = idx; i < bulletCount; ++i) {

        bullets[i] = bullets[i + 1];                                        // This line replaces the current bullet with the next bullet. The bullet at index i is replaced by the bullet at index i + 1.
                                                                            // The previous bullet is replaced by the next bullet and is thus deleted.
    }

    bulletCount--;                                                          // The total number of bullets is reduced by one.
}



// Checks if the enemy is in contact with the player
int playerCollision() {

    if (enemy.x == 4  &&  enemy.y == playerY + 1) { return 1; }
    // It means that the enemy is in contact with the player's character.


    if (enemy.x == 3  &&  (enemy.y == playerY  ||  enemy.y == playerY + 2)) { return 1; }
    // It means that the enemy is in contact with the player's character.


    if (enemy.x == 1  &&  (enemy.y == playerY + 3  ||  enemy.y == playerY - 1))  { return 1;}
    // It means that the enemy is in contact with the player's character.


    return 0;
    // It means that the enemy is not in contact with the player's character.

}



// Checks if a bullet exists at the specified location
int IsBullet(int x, int y) {

    for (int i = 0; i < bulletCount; i++) {

        if (bullets[i].x == x  &&  bullets[i].y == y) {

            return 1;
        }

    }

    return 0;
}





// Function to display the end screen
void end_screen(void) // bitis ekrani
{
  while (1)
  {
    clear_screen();
  }
}



// Main function
void kmain(void)
{
    Start();

    while (1) {

        
        Draw();
        keyboard_handler_main();

        if (FIRE != 0) {

            FIRE--;             // If the fire is non-zero, subtract 1 from the fire variable to indicate that a bullet has been fired and the number of bullets has decreased.

        }

        sleep(13000000);           // Wait SPEED milliseconds.

    }

    Draw();
}