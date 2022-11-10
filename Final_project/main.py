from ili9341 import Display, color565
from machine import Pin, SPI
from machine import Pin, PWM
from xglcd_font import XglcdFont
import time

spi = SPI(0,
          baudrate=31250000,
          polarity=1,
          phase=1,
          bits=8,
          firstbit=SPI.MSB,
          sck=Pin(6),
          mosi=Pin(7),
          miso=Pin(4))
display = Display(spi, dc=Pin(15), cs=Pin(13), rst=Pin(14),
                          width=320, height=240,
                          rotation=90)
arcadepix = XglcdFont('fonts/ArcadePix9x11.c', 9, 11)
buzzer = PWM(Pin(16))

#time.sleep(5)
buzzer.duty_u16(0)
left = Pin(8, Pin.IN, Pin.PULL_UP)
right = Pin(10, Pin.IN, Pin.PULL_UP)

# coordinates of the paddle on the screen in pixels
# the screen is 128 pixels wide by 64 pixel high
xp = 140
yp = 236

x = 160 # ball coordinates on the screen in pixels
y = 0
vx = 1 # ball velocity along x and y in pixels per frame
vy = 1
old_x = 0
old_y = 0
old_xp = 0
old_yp = 0
score = 0
while True:
    buzzer.duty_u16(0) # 16384 = 25% duty cycle
    
    display.fill_rectangle(old_xp,old_yp,50,4,color=0)
    display.fill_rectangle(xp, yp, 50, 4, color565(150, 150, 150))
    old_xp = xp
    old_yp = yp
    if left.value() == 0:
        #print("LEFT Button Pressed")
        xp = xp - 1 # Move the paddle to the left by 1 pixel
    elif right.value() == 0:
        #print("RIGHT Button Pressed")
        xp = xp + 1 # Move the paddle to the right by 1 pixel
    # Clear the screen
    display.fill_rectangle(old_x,old_y,10,10,color=0)
    
    # Draw a 4x4 pixels ball at (x,y) in white
    display.fill_rectangle(x, y, 10, 10, color565(150, 150, 150))
    
    old_x = x
    old_y = y
    
    # Move the ball by adding the velocity vector
    x += vx
    y += vy
    
    # Make the ball rebound on the edges of the screen
    if x < 0:
        x = 0
        vx = -vx
        buzzer.duty_u16(32768) # 16384 = 25% duty cycle
        buzzer.freq(1300)
        time.sleep(0.005)
    if y < 0:
        y = 0
        vy = -vy
        buzzer.duty_u16(32768) # 16384 = 25% duty cycle
        buzzer.freq(1300)
        time.sleep(0.005)
    if x + 10 > 320:
        x = 320 - 10
        vx = -vx
        buzzer.duty_u16(32768) # 16384 = 25% duty cycle
        buzzer.freq(1300)
        time.sleep(0.005)
    if y + 10 > 240:
        if x < xp + 50 and x > xp:
            y = 240 - 10
            vy = -vy
            score += 1
            buzzer.duty_u16(32768) # 16384 = 25% duty cycle
            buzzer.freq(1300)
            time.sleep(0.005)
            display.draw_text(250, 0, 'Score: '+str(score), arcadepix, color565(255, 0, 0))
        else:
            print("Game Over")
            display.draw_text(120, 120, 'Game Over', arcadepix, color565(255, 0, 0))
            buzzer.duty_u16(32768) # 16384 = 25% duty cycle
            buzzer.freq(1300)
            time.sleep(0.1)
            buzzer.freq(1500)
            time.sleep(0.1)
            buzzer.freq(1000)
            time.sleep(0.1)
            buzzer.duty_u16(0)
            break