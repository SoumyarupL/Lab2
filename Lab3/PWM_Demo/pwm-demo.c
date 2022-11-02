/**
 * 
 * HARDWARE CONNECTIONS
 *   - GPIO 5 ---> PWM Channel B output
 *   - GPIO 4 ---> PWM Channel A output
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/i2c.h"
// Include custom libraries
#include "vga_graphics.h"
#include "mpu6050.h"

#include "pt_cornell_rp2040_v1.h"

// Arrays in which raw measurements will be stored
fix15 acceleration[3], gyro[3];

// character array
char screentext[40];

// draw speed
int threshold = 10 ;

// Some macros for max/min/abs
#define min(a,b) ((a<b) ? a:b)
#define max(a,b) ((a<b) ? b:a)
#define abs(a) ((a>0) ? a:-a)

// semaphore
static float pid = 0;
static struct pt_sem vga_semaphore ;
static fix15 error = float2fix15(0);
static fix15 error_deriv = float2fix15(0);
static fix15 prev_error = float2fix15(0);
static int kp = 600;
static int ki = 2;
static int kd = 9000;
static fix15 desired_angle = 0;
static fix15 angle_increment = float2fix15(0.0001);
static char debug[60];
static int motor_disp = 0;

// PWM wrap value and clock divide value
// For a CPU rate of 125 MHz, this gives
// a PWM frequency of 1 kHz.
#define WRAPVAL 5000
#define CLKDIV 25.0f

// Variable to hold PWM slice number
uint slice_num0 ;
uint slice_num1 ;

int pwm_no = 0;

// PWM duty cycle
volatile int control0 ;
volatile int old_control0 ;
volatile int control1 ;
volatile int old_control1 ;
static fix15 accel_angle;
static fix15 gyro_angle_delta;
static fix15 complementary_angle;
static fix15 error_accum;
fix15 Imax = int2fix15(2500);

// PWM interrupt service routine
void on_pwm_wrap() {
        mpu6050_read_raw(acceleration, gyro);
        gyro_angle_delta = multfix15(gyro[2], zeropt001) ;

        accel_angle = -multfix15(divfix(acceleration[0], acceleration[1]), oneeightyoverpi) ;

        complementary_angle = multfix15(complementary_angle - gyro_angle_delta, zeropt999) + multfix15(accel_angle, zeropt001);
        error = desired_angle - complementary_angle;
        if (error<int2fix15(0)){
            desired_angle -= angle_increment;
        }
        else{
            desired_angle += angle_increment;
        }

        error_deriv = error - prev_error;
        error_accum += error;
        
        
        if (error_accum > Imax) error_accum = Imax;
        else if (error_accum < -Imax) error_accum = -Imax;
        
        pid = ((kp * fix2float15(error)) + ki*fix2float15(error_accum) + kd*fix2float15(error_deriv) ); 
        prev_error = error;
        
        if (pid>0){
            pwm_no = 0;
            control0 = (int) (pid); 
            motor_disp = motor_disp + ((control0 - motor_disp)>>6);
        }
        else{
            pwm_no = 1;
            control1 = (-1)*(int) (pid);
            motor_disp = motor_disp + ((control1 - motor_disp)>>6);
        }
    if(pwm_no==0){
        // Clear the interrupt flag that brought us here
        pwm_clear_irq(pwm_gpio_to_slice_num(5));
        control1 = 0;
        // Update duty cycle
        if (control0!=old_control0) {
            old_control0 = control0 ;
            pwm_set_chan_level(slice_num1, PWM_CHAN_A, control1);
            pwm_set_chan_level(slice_num0, PWM_CHAN_B, control0);
        }
    }
    else{
        // Clear the interrupt flag that brought us here
        pwm_clear_irq(pwm_gpio_to_slice_num(4));
        // Update duty cycle
        control0 = 0;
        if (control1!=old_control1) {
            old_control1 = control1 ;
            pwm_set_chan_level(slice_num0, PWM_CHAN_B, control0);
            pwm_set_chan_level(slice_num1, PWM_CHAN_A, control1);
        }
    }
     // Read the IMU
    // NOTE! This is in 15.16 fixed point. Accel in g's, gyro in deg/s
    // If you want these values in floating point, call fix2float15() on
    // the raw measurements.
    

    // Signal VGA to draw
    PT_SEM_SIGNAL(pt, &vga_semaphore);
}
static PT_THREAD (protothread_vga(struct pt *pt))
{
    // Indicate start of thread
    PT_BEGIN(pt) ;
    

    // We will start drawing at column 81
    static int xcoord = 81 ;
    
    // Rescale the measurements for display
    static float OldRange = 500. ; // (+/- 250)
    static float NewRange = 150. ; // (looks nice on VGA)
    static float OldMin = -250. ;
    static float OldMax = 250. ;

    // Control rate of drawing
    static int throttle ;
    static char control[60];

    // Draw bottom plot
    drawHLine(75, 430, 5, CYAN) ;
    drawHLine(75, 355, 5, CYAN) ;
    drawHLine(75, 280, 5, CYAN) ;
    drawVLine(80, 280, 150, CYAN) ;
    sprintf(screentext, "0") ;
    setCursor(50, 350) ;
    writeString(screentext) ;
    sprintf(screentext, "+2") ;
    setCursor(50, 280) ;
    writeString(screentext) ;
    sprintf(screentext, "-2") ;
    setCursor(50, 425) ;
    writeString(screentext) ;

    // Draw top plot
    drawHLine(75, 230, 5, CYAN) ;
    drawHLine(75, 155, 150, CYAN) ;
    drawHLine(75, 80, 5, CYAN) ;
    drawVLine(80, 80, 150, CYAN) ;
    sprintf(screentext, "0") ;
    setCursor(50, 150) ;
    writeString(screentext) ;
    sprintf(screentext, "+250") ;
    setCursor(45, 75) ;
    writeString(screentext) ;
    sprintf(screentext, "-250") ;
    setCursor(45, 225) ;
    writeString(screentext) ;
    

    while (true) {
        // Wait on semaphore
        PT_SEM_WAIT(pt, &vga_semaphore);

        
        // Increment drawspeed controller
        throttle += 1 ;
        // If the controller has exceeded a threshold, draw
        if (throttle >= threshold) { 
            // Zero drawspeed controller
            throttle = 0 ;

            setTextColor(WHITE) ;
            fillRect(65, 10, 175, 10, BLACK);
            setCursor(65, 10) ;
            setTextSize(1) ;
            writeString(debug) ;

            // Erase a column
            drawVLine(xcoord, 0, 480, BLACK) ;
            drawHLine(75, 155, 450, CYAN) ;

            // Draw bottom plot (multiply by 120 to scale from +/-2 to +/-250)
            drawPixel(xcoord, 430 - (int)(NewRange*((float)((fix2float15(complementary_angle))-OldMin)/OldRange)), WHITE) ;
            drawPixel(xcoord, 430 - (int)(NewRange*((float)((fix2float15(accel_angle))-OldMin)/OldRange)), RED) ;
            //drawPixel(xcoord, 430 - (int)(NewRange*((float)((fix2float15(acceleration[2])*120.0)-OldMin)/OldRange)), GREEN) ;

            // Draw top plot
            drawPixel(xcoord, 230 - (int)(NewRange*((float)(((kp))-OldMin)/OldRange)), WHITE) ;
            if (pwm_no == 0){
                drawPixel(xcoord, 230 - (int)(NewRange*((float)(((int)motor_disp/50)-OldMin)/OldRange)), RED) ;
            }
            else{
                drawPixel(xcoord, 230 - (int)(NewRange*((float)((-(int)motor_disp/50)-OldMin)/OldRange)), GREEN) ;
            }

            // Update horizontal cursor
            if (xcoord < 609) {
                xcoord += 1 ;
            }
            else {
                xcoord = 81 ;
            }
        }
    }
    // Indicate end of thread
    PT_END(pt);
}


// Entry point for core 1
void core1_entry() {
    pt_add_thread(protothread_vga) ;
    pt_schedule_start ;
}

// User input thread
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;
    static int test_in ;
    while(1) {
        sprintf(pt_serial_out_buffer, "Input Kp,Ki,Kd:");
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer,"%d,%d,%d",&kp,&ki,&kd) ;
    }
    PT_END(pt) ;
}


int main() {

    // Initialize stdio
    stdio_init_all();
    // Initialize VGA
    initVGA() ;

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// I2C CONFIGURATION ////////////////////////////
    i2c_init(I2C_CHAN, I2C_BAUD_RATE) ;
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C) ;
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C) ;
    gpio_pull_up(SDA_PIN) ;
    gpio_pull_up(SCL_PIN) ;

    // MPU6050 initialization
    mpu6050_reset();
    mpu6050_read_raw(acceleration, gyro);
    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// PWM CONFIGURATION ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Tell GPIO 5 that it is allocated to the PWM
    gpio_set_function(5, GPIO_FUNC_PWM);
    
    gpio_set_function(4, GPIO_FUNC_PWM);
    slice_num1 = pwm_gpio_to_slice_num(4);
    

    // Find out which PWM slice is connected to GPIO 5 (it's slice 2)
    slice_num0 = pwm_gpio_to_slice_num(5);
    
    
        // Mask our slice's IRQ output into the PWM block's single interrupt line,
        // and register our interrupt handler
        pwm_clear_irq(slice_num0);
        pwm_set_irq_enabled(slice_num0, true);
        irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
        irq_set_enabled(PWM_IRQ_WRAP, true);

        // This section configures the period of the PWM signals
        pwm_set_wrap(slice_num0, WRAPVAL) ;
        pwm_set_clkdiv(slice_num0, CLKDIV) ;
    if (pwm_no == 0){
        // This sets duty cycle
        pwm_set_chan_level(slice_num0, PWM_CHAN_B, 3125);

        // Start the channel
        pwm_set_mask_enabled((1u << slice_num0));
    }
    
    else{
        pwm_set_chan_level(slice_num1, PWM_CHAN_A, 3125);

        // Start the channel
        pwm_set_mask_enabled((1u << slice_num1));
    }
    ////////////////////////////////////////////////////////////////////////
    ///////////////////////////// ROCK AND ROLL ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);

    pt_add_thread(protothread_serial) ;
    pt_schedule_start ;

}
