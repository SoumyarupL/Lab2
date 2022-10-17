
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

spin_lock_t * lock_update;

float bias = 0.001;
fix15 turn_factor = float2fix15(0.2);
fix15 visual_range = float2fix15(40);
fix15 protected_range = float2fix15(8);
fix15 centering_factor = float2fix15(0.0005);
fix15 avoid_factor = float2fix15(0.05);
fix15 matching_factor = float2fix15(0.05);
fix15 max_bias = float2fix15(0.01);
fix15 bias_increment = float2fix15(0.00004);
fix15 biasval = float2fix15(0.001);
fix15 maxspeed = int2fix15(6);
fix15 minspeed = int2fix15(3);
fix15 zero_point_4 = float2fix15(0.4) ;
#define num_boids 700



#define BOTTOM int2fix15(380)
#define TOP int2fix15(100)
#define LEFT int2fix15(100)
#define RIGHT int2fix15(540)

// uS per frame
#define FRAME_RATE 33000

struct boid{
  fix15 x;
  fix15 y;
  fix15 vx;
  fix15 vy;
  fix15 sg;
  fix15 bias;
};

// the color of the boid
char color = WHITE ;

int sg1;
int sg2;
int sg3;

static struct boid swarm[num_boids];

int top = 100, bottom = 380, right = 540, left = 100;

// Create a boid
void spawnBoid(struct boid* swarm, int num, int direction)
{
  for (int i = 0; i<num; i++){
    swarm[i].x = int2fix15(100 + rand()%540 + 1) ;
    swarm[i].y = int2fix15(100 + rand()%380 + 1) ;
    if (direction) swarm[i].vx = int2fix15(5) ;
    else swarm[i].vx = int2fix15(-1*(5)) ;
    swarm[i].vy = int2fix15(5);
    swarm[i].bias = biasval;
    if (i<num/3) swarm[i].sg = int2fix15(1);
    else if (i<2*num/3 && i>num/3) swarm[i].sg = int2fix15(2);
    else swarm[i].sg = int2fix15(3);
  }
}

// Draw the boundaries
void drawArena() {
  if ((top>10 && bottom<470) && (left>10 && right<630)){
  drawVLine(left, top, (bottom - top), WHITE) ;
  drawVLine(right, top, (bottom - top), WHITE) ;
  drawHLine(left, top, (right - left), WHITE) ;
  drawHLine(left, bottom, (right - left), WHITE) ;
  }
  else if((left>10 && right<630)&&(top<10 && bottom>470)){
    drawVLine(left, top, 480, WHITE) ;
    drawVLine(right, top, 480, WHITE) ;
  }
  else if ((top>10 && bottom<470)&&(left<10 && right>630)){
    
    drawHLine(left, top, (right - left), WHITE) ;
    drawHLine(left, bottom, (right - left), WHITE) ;
  }
}

void drawInfo(int spare_time){
  int s_time;
 if (spare_time < 0)s_time = 0;
 else s_time = spare_time;
  static int current_time;
  current_time = (time_us_32()/1000000);
  static char num_b[20];
  sprintf(num_b, "Number of Boids: %d", num_boids) ;
  static char freqtext[60];
  sprintf(freqtext, "%d seconds", current_time) ;
  static char timing[40];
  sprintf(timing, "%d microseconds", s_time) ;
  // Write some text to VGA
    setTextColor(WHITE) ;
    setCursor(65, 0) ;
    setTextSize(1) ;
    writeString(num_b) ;
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

void update_boid(struct boid* swarm,int begin, int end, int num){
  for (int i = begin; i<end; i++){
    //erase old boid
    drawRect(fix2int15(swarm[i].x), fix2int15(swarm[i].y), 2, 2, BLACK);
    if(swarm[i].y >= int2fix15(479) && (top==0 && bottom == 480) ){
      swarm[i].y = swarm[i].y - int2fix15(479);
    }
    if(swarm[i].y <= int2fix15(0) && (top==0 && bottom == 480)){
      swarm[i].y = swarm[i].y + int2fix15(479);
    }
    if(swarm[i].x >= int2fix15(639) && (left==0 && right == 640)){
      swarm[i].x = swarm[i].x - int2fix15(639);
    }
    if(swarm[i].x <= int2fix15(0) && (left==0 && right == 640)){
      swarm[i].x = swarm[i].x + int2fix15(639);
    }
    fix15 xpos_avg = 0, ypos_avg = 0, xvel_avg = 0, yvel_avg = 0, neighboring_boids = 0, close_dx = 0, close_dy = 0;
    for (int j = 0; j<num; j++){
    if (j!=i){
      fix15 dx = swarm[i].x - swarm[j].x;
      fix15 dy = swarm[i].y - swarm[j].y;
      if (absfix15(dx)<visual_range && absfix15(dy)<visual_range){
        fix15 squared_dist = multfix15(dx,dx) + multfix15(dy,dy);
        if (squared_dist<multfix15(protected_range,protected_range)){
          close_dx += swarm[i].x - swarm[j].x;
          close_dy += swarm[i].y - swarm[j].y;
        }
        else if (squared_dist<multfix15(visual_range,visual_range)){
          xpos_avg += swarm[j].x;
          ypos_avg += swarm[j].y;
          xvel_avg += swarm[j].vx;
          yvel_avg += swarm[j].vy;
          neighboring_boids += 1;
        }
      }
    }
    }
    if (neighboring_boids>0){
      xpos_avg = (xpos_avg/neighboring_boids);
      ypos_avg = (ypos_avg/neighboring_boids);
      xvel_avg = (xvel_avg/neighboring_boids);
      yvel_avg = (yvel_avg/neighboring_boids);

      swarm[i].vx = (swarm[i].vx + multfix15((xpos_avg - swarm[i].x),centering_factor) + 
                   multfix15((xvel_avg - swarm[i].vx),matching_factor));
      swarm[i].vy = (swarm[i].vy + multfix15((ypos_avg - swarm[i].y),centering_factor) + 
                   multfix15((yvel_avg - swarm[i].vy),matching_factor));
    }

    swarm[i].vx = swarm[i].vx + multfix15(close_dx,avoid_factor);
    swarm[i].vy = swarm[i].vy + multfix15(close_dy,avoid_factor);


    if (swarm[i].y<int2fix15(top)) {
    swarm[i].vy = (swarm[i].vy + turn_factor) ;
    }
    if (swarm[i].y>int2fix15(bottom)) {
      swarm[i].vy = (swarm[i].vy - turn_factor) ;
    } 
    if (swarm[i].x>int2fix15(right)) {
      swarm[i].vx = (swarm[i].vx - turn_factor) ;
    }
    if(swarm[i].x<int2fix15(left)) {
      swarm[i].vx = (swarm[i].vx + turn_factor) ;
    }
    
    swarm[i].bias = biasval;

    if (swarm[i].sg == int2fix15(1)){
      if (swarm[i].vx > 0) swarm[i].bias = fmin(max_bias, swarm[i].bias + bias_increment);
      else swarm[i].bias = fmax(bias_increment, swarm[i].bias - bias_increment);
      swarm[i].vx = multfix15((int2fix15(1) - swarm[i].bias),swarm[i].vx) + swarm[i].bias;
    }

    if (swarm[i].sg == int2fix15(2)){
      if (swarm[i].vx < 0) swarm[i].bias = fmin(max_bias, swarm[i].bias + bias_increment);
      else swarm[i].bias = fmax(bias_increment, swarm[i].bias + bias_increment);
      swarm[i].vx = multfix15((int2fix15(1) - swarm[i].bias),swarm[i].vx) - swarm[i].bias;
    }

    fix15 speed = fmax(absfix15(swarm[i].vx),absfix15(swarm[i].vy)) + multfix15(zero_point_4,fmin(absfix15(swarm[i].vx),absfix15(swarm[i].vy)));
  

    if (speed < minspeed){
        swarm[i].vx = multfix15(divfix(swarm[i].vx,speed),minspeed);
        swarm[i].vy = multfix15(divfix(swarm[i].vy,speed),minspeed);
    }
    if (speed > maxspeed){
        swarm[i].vx = multfix15(divfix(swarm[i].vx,speed),maxspeed);
        swarm[i].vy = multfix15(divfix(swarm[i].vy,speed),maxspeed);
    }
    //printf("Update0: %d,%d,%d,%d\n",fix2int15(swarm[i].x),fix2int15(swarm[i].y),fix2int15(swarm[i].vx),fix2int15(swarm[i].vy));

    swarm[i].x = swarm[i].x + swarm[i].vx;
    swarm[i].y = swarm[i].y + swarm[i].vy;

    //draw boid at updated position
    if (swarm[i].sg == int2fix15(1))drawRect(fix2int15(swarm[i].x), fix2int15(swarm[i].y), 2, 2, WHITE);
    else if (swarm[i].sg == int2fix15(2))drawRect(fix2int15(swarm[i].x), fix2int15(swarm[i].y), 2, 2, RED);
    else drawRect(fix2int15(swarm[i].x), fix2int15(swarm[i].y), 2, 2, BLUE);

  }
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
    static int top_old, bottom_old, right_old,left_old;
      while(1) {
        // print prompt
        sprintf(pt_serial_out_buffer, "Enter:");
        // non-blocking write
        serial_write ;
        top_old = top;
        bottom_old = bottom;
        right_old = right;
        left_old = left;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer,"%d,%d,%d,%d,%f,%d,%d,%d", &top, &bottom, &right, &left, &bias, &sg1, &sg2, &sg3) ;
        biasval = float2fix15(bias);
        
        // update boid color
        if (pt_serial_in_buffer){
          for (int i = 0; i<sg1; i++){
          swarm[i].sg = int2fix15(1);
        }
        for (int i = sg1; i<sg2; i++){
          swarm[i].sg = int2fix15(2);
        }
        for (int i = sg2; i<sg3; i++){
          swarm[i].sg = int2fix15(3);
        }
          drawVLine(left_old, top_old, (bottom_old - top_old), BLACK) ;
          drawVLine(right_old, top_old, (bottom_old - top_old), BLACK) ;
          drawHLine(left_old, top_old, (right_old - left_old), BLACK) ;
          drawHLine(left_old, bottom_old, (right_old - left_old), BLACK) ;
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
    
    // Spawn a boid
    spawnBoid(swarm, num_boids, 0);

    while(1) {
        // Measure time at start of thread
        begin_time = time_us_32() ;
        PT_LOCK_WAIT(pt, lock_update) ;
        update_boid(swarm, 0, num_boids/2, num_boids);
        PT_LOCK_RELEASE(lock_update);
        // draw the boundaries
        drawArena() ;
        // delay in accordance with frame rate
        spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
        drawInfo(spare_time);
        // yield for necessary amount of time
        PT_YIELD_usec(spare_time) ;
      // NEVER exit while
      // END WHILE(1)
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

    while(1) {
      begin_time = time_us_32() ;   
        // update boid's position and velocity
        PT_LOCK_WAIT(pt, lock_update) ;
        update_boid(swarm, num_boids/2, num_boids, num_boids);
        PT_LOCK_RELEASE(lock_update);
        // draw the boundaries
        //drawArena() ;
        // delay in accordance with frame rate
        spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
        //drawInfo(spare_time);
        // yield for necessary amount of time
        PT_YIELD_usec(spare_time) ;
    } // END WHILE(1)
  PT_END(pt);
} // animation thread

// ========================================
// === core 1 main -- started in main below
// ========================================
void core1_main(){
  // Add animation thread
  pt_add_thread(protothread_serial);
  pt_add_thread(protothread_anim1);
  // Start the scheduler
  pt_schedule_start ;

}

// ========================================
// === main
// ========================================
// USE ONLY C-sdk library
int main(){
  set_sys_clock_khz(250000, true);
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
