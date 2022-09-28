
/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 * This demonstration animates two balls bouncing about the screen.
 * Through a serial interface, the user can change the ball color.
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 330 ohm resistor ---> VGA Red
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - RP2040 GND ---> VGA GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels 0, 1
 *  - 153.6 kBytes of RAM (for pixel color data)
 *
 */

// Include the VGA grahics library
#include "vga_graphics.h"
// Include standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
// Include Pico libraries
#include "pico/stdlib.h"
#include "pico/divider.h"
#include "pico/multicore.h"
// Include hardware libraries
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"

// Include protothreads
#include "pt_cornell_rp2040_v1.h"

 #define maxspeed 6
 #define minspeed 3

 volatile int frame_counter = 0;

// === the fixed point macros ========================================
typedef signed int fix15 ;
#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))
#define float2fix15(a) ((fix15)((a)*32768.0)) // 2^15
#define fix2float15(a) ((float)(a)/32768.0)
#define absfix15(a) abs(a) 
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)
#define divfix(a,b) (fix15)(div_s64s64( (((signed long long)(a)) << 15), ((signed long long)(b))))

// Wall detection
#define hitBottom(b) (b>int2fix15(380))
#define hitTop(b) (b<int2fix15(100))
#define hitLeft(a) (a<int2fix15(100))
#define hitRight(a) (a>int2fix15(540))

#define BOTTOM int2fix15(380)
#define TOP int2fix15(100)
#define LEFT int2fix15(100)
#define RIGHT int2fix15(540)

// uS per frame
#define FRAME_RATE 33000

fix15 turn_factor = float2fix15(1) ;

struct boid{
  fix15 x;
  fix15 y;
  fix15 vx;
  fix15 vy;
};

// the color of the boid
char color = WHITE ;

// Boid on core 0
fix15 boid0_x ;
fix15 boid0_y ;
fix15 boid0_vx ;
fix15 boid0_vy ;

// Boid on core 1
fix15 boid1_x ;
fix15 boid1_y ;
fix15 boid1_vx ;
fix15 boid1_vy ;

// Create a boid
void spawnBoid(struct boid* swarm, int num, int direction)
{
  for (int i = 0; i<num; i++){
    swarm[i].x = int2fix15(100 + rand()%540 + 1) ;
    swarm[i].y = int2fix15(100 + rand()%380 + 1) ;
    if (direction) swarm[i].vx = int2fix15(minspeed + rand()%(maxspeed-minspeed)) ;
    else swarm[i].vx = int2fix15(-1*(minspeed + rand()%(maxspeed-minspeed))) ;
    swarm[i].vy = int2fix15(minspeed + rand()%(maxspeed-minspeed));
  }
}

// Draw the boundaries
void drawArena() {
  drawVLine(100, 100, 280, WHITE) ;
  drawVLine(540, 100, 280, WHITE) ;
  drawHLine(100, 100, 440, WHITE) ;
  drawHLine(100, 380, 440, WHITE) ;
}

void drawInfo(int spare_time){
 
  static int current_time;
  current_time = (time_us_32()/1000000);
  static char freqtext[60];
  sprintf(freqtext, "%d seconds", current_time) ;
  static char timing[40];
  sprintf(timing, "%d microseconds", spare_time) ;
  // Write some text to VGA
    setTextColor(WHITE) ;
    setCursor(65, 0) ;
    setTextSize(1) ;
    writeString("Number of boids : 1") ;
    setCursor(65, 10) ;
    writeString("Spare Time:") ;
    fillRect(140, 10, 250, 10, BLACK);
    setCursor(140, 10) ;
    writeString(timing) ;
    setCursor(65, 20) ;
    writeString("Time (in seconds):") ;
    fillRect(65, 30, 176, 30, BLACK); // red box
    setCursor(65, 30) ;
    writeString(freqtext) ;
}

// Detect wallstrikes, update velocity and position
void wallsAndEdges(fix15 x, fix15 y, fix15 vx, fix15 vy)
{
  // Reverse direction if we've hit a wall
  if (hitTop(*y)) {
    *vy = (*vy + turn_factor) ;
    //*y  = (*y + int2fix15(5)) ;
  }
  if (hitBottom(*y)) {
    *vy = (*vy - turn_factor) ;
    //*y  = (*y - int2fix15(5)) ;
  } 
  if (hitRight(*x)) {
    *vx = (*vx - turn_factor) ;
    //*x  = (*x - int2fix15(5)) ;
  }
  if (hitLeft(*x)) {
    *vx = (*vx + turn_factor) ;
    //*x  = (*x + int2fix15(5)) ;
  } 
  /*if (abs(*vx)>maxspeed){
    if (*vx>0) *vx = maxspeed;
    else *vx = -maxspeed;
  }
  if (abs(*vy)>maxspeed){
    if (*vy>0) *vy = maxspeed;
    else *vy = -maxspeed;
  }
  if (abs(*vx)<minspeed){
    if (*vx>0) *vx = minspeed;
    else *vx = -minspeed;
  }
  if (abs(*vy)<minspeed){
    if (*vy>0) *vy = minspeed;
    else *vy = -minspeed;
  }*/
  
  // Update position using velocity
  *x = *x + *vx ;
  *y = *y + *vy ;
}

// ==================================================
// === users serial input thread
// ==================================================
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);
    // stores user input
    static int user_input ;
    // wait for 0.1 sec
    PT_YIELD_usec(1000000) ;
    // announce the threader version
    sprintf(pt_serial_out_buffer, "Protothreads RP2040 v1.0\n\r");
    // non-blocking write
    serial_write ;
      while(1) {
        // print prompt
        sprintf(pt_serial_out_buffer, "input a number in the range 1-7: ");
        // non-blocking write
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer,"%d", &user_input) ;
        // update boid color
        if ((user_input > 0) && (user_input < 8)) {
          color = (char)user_input ;
        }
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// Animation on core 0
static PT_THREAD (protothread_anim(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining frame rate
    static int begin_time ;
    static int spare_time ;
    struct boid swarm[10];
    // Spawn a boid
    spawnBoid(swarm, 10, 0);

    while(1) {
      for (int i = 0; i<10; i++){
        // Measure time at start of thread
        begin_time = time_us_32() ;      
        // erase boid
        drawRect(fix2int15(swarm[i].x), fix2int15(swarm[i].y), 2, 2, BLACK);
        // update boid's position and velocity
        wallsAndEdges(swarm[i].x, swarm[i].y, swarm[i].vx, swarm[i].vy) ;
        // draw the boid at its new position
        drawRect(fix2int15(swarm[i].x), fix2int15(swarm[i].y), 2, 2, color); 
        // draw the boundaries
        drawArena() ;
        // delay in accordance with frame rate
        spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
        drawInfo(spare_time);
        // yield for necessary amount of time
        PT_YIELD_usec(spare_time) ;
      // NEVER exit while
    } // END WHILE(1)
    }
  PT_END(pt);
} // animation thread


// Animation on core 1
static PT_THREAD (protothread_anim1(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining frame rate
    static int begin_time ;
    static int spare_time ;

    // Spawn a boid
    spawnBoid(&boid1_x, &boid1_y, &boid1_vx, &boid1_vy, 1);

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32() ;      
      // erase boid
      drawRect(fix2int15(boid1_x), fix2int15(boid1_y), 2, 2, BLACK);
      // update boid's position and velocity
      wallsAndEdges(&boid1_x, &boid1_y, &boid1_vx, &boid1_vy) ;
      // draw the boid at its new position
      drawRect(fix2int15(boid1_x), fix2int15(boid1_y), 2, 2, color); 
      // delay in accordance with frame rate
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
      // yield for necessary amount of time
      PT_YIELD_usec(spare_time) ;
     // NEVER exit while
    } // END WHILE(1)
  PT_END(pt);
} // animation thread

// ========================================
// === core 1 main -- started in main below
// ========================================
void core1_main(){
  // Add animation thread
  pt_add_thread(protothread_serial);
  // Start the scheduler
  pt_schedule_start ;

}

// ========================================
// === main
// ========================================
// USE ONLY C-sdk library
int main(){
  // initialize stio
  stdio_init_all() ;

  // initialize VGA
  initVGA() ;
  srand(time(NULL));
  
  // start core 1 
  multicore_reset_core1();
  multicore_launch_core1(&core1_main);

  // add threads
  pt_add_thread(protothread_anim);

  // start scheduler
  pt_schedule_start ;
} 
