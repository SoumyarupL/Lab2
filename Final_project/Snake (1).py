# Original source code from https://github.com/Twan37/PicoSnake

from machine import Pin, I2C, Timer, SPI, PWM
from ili9341 import Display, color565
import utime
import urandom
from xglcd_font import XglcdFont



# Screen
spi = SPI (0,
           baudrate=31250000,
           polarity=1,
           phase=1,
           bits=8,
           firstbit=SPI.MSB,
           sck=Pin(4),
           mosi=Pin(5),
           miso=Pin(2))

display = Display(spi, dc=Pin(15), cs=Pin(13), rst=Pin(14),
                  width=320, height=240,rotation=90)

speaker = PWM(Pin(18))
buzzer = PWM(Pin(16))
SCREEN_HEIGHT = 240
SCREEN_WIDTH = 320
SEGMENT_WIDTH = 8

# time.sleep(5)
buzzer.duty_u16(0)
up = Pin(5, Pin.IN, Pin.PULL_UP)
down = Pin(9, Pin.IN, Pin.PULL_UP)
left = Pin(8, Pin.IN, Pin.PULL_UP)
right = Pin(12, Pin.IN, Pin.PULL_UP)

arcadepix = XglcdFont('fonts/ArcadePix9x11.c', 9, 11)



#coordinates where the snake will be moving on the screen
xp = 140  
yp = 236

SEGMENT_PIXELS = int(SCREEN_HEIGHT/SEGMENT_WIDTH) # 30
SEGMENTS_HIGH = int(SCREEN_HEIGHT/SEGMENT_WIDTH) # 30
SEGMENTS_WIDE = int(SCREEN_WIDTH/SEGMENT_WIDTH) # 40
VALID_RANGE = [[int(i /SEGMENTS_HIGH), i % SEGMENTS_HIGH] for i in range(SEGMENTS_WIDE * SEGMENTS_HIGH -1)]

# Game code
game_timer = Timer()
player = None
food = None

class Snake:
    up = 0
    down = 1
    left = 2
    right = 3
    
    def __init__(self, x=int(SEGMENTS_WIDE/2), y=int(SEGMENTS_HIGH/2) + 1):
        self.segments = [[x, y]]
        self.x = x
        self.y = y
        self.dir = urandom.randint(0,3)
        self.state = True
        
    def reset(self, x=int(SEGMENTS_WIDE/2), y=int(SEGMENTS_HIGH/2) + 1):
        self.segments = [[x, y]]
        self.x = x
        self.y = y
        self.dir = urandom.randint(0,3)
        self.state = True
        
    def move(self):
        new_x = self.x
        new_y = self.y
        
        if self.dir == Snake.up:
            new_y -= 1
        elif self.dir == Snake.down:
            new_y += 1
        elif self.dir == Snake.left:
            new_x -= 1
        elif self.dir == Snake.right:
            new_x += 1
        
        for i, _ in enumerate(self.segments):
            if i != len(self.segments) - 1:
                self.segments[i][0] = self.segments[i+1][0]
                self.segments[i][1] = self.segments[i+1][1]
        
        if self._check_crash(new_x, new_y):
            # Oh no, we killed the snake :C
            if self.state == True:
                # play an ugly sound
                speaker.freq(200)
                speaker.duty_u16(2000)
                utime.sleep(0.5)
                speaker.duty_u16(0)
            self.state = False
        
        self.x = new_x
        self.y = new_y
        
        self.segments[-1][0] = self.x
        self.segments[-1][1] = self.y
        
    def eat(self):
        display.fill_rectangle(self.x * SEGMENT_PIXELS, self.y * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 0) # black
        display.fill_rectangle(self.x * SEGMENT_PIXELS, self.y * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, color565(255,255,255)) # white
        self.segments.append([self.x, self.y])
        # Make a sound
        speaker.freq(1000)
        speaker.duty_u16(2000)
        utime.sleep(0.100)
        speaker.duty_u16(0)
        
    def change_dir(self, dir):
        if  dir == Snake.down and self.dir == Snake.up:
            return False
        
        elif dir == Snake.up and self.dir == Snake.down:
            return False
        
        elif dir == Snake.right and self.dir == Snake.left:
            return False
        
        elif dir == Snake.left and self.dir == Snake.right:
            return False
        
        self.dir = dir
        
    def _check_crash(self, new_x, new_y):
        if new_y >= SEGMENTS_HIGH or new_y < 0 or new_x >= SEGMENTS_WIDE or new_x < 0 or [new_x, new_y] in self.segments:
            return True
        else:
            return False
    
    def draw(self):
        display.fill_rectangle(self.segments[-1][0] * SEGMENT_PIXELS, self.segments[-1][1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, color565(255,255,255))#white


def main():
    global player
    global food
    
    player = Snake()
    food = urandom.choice([coord for coord in VALID_RANGE if coord not in player.segments])
    display.fill_rectangle(food[0] * SEGMENT_PIXELS , food[1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, color565(255,255,255))# white
    
    # Playing around with this cool timer.
    game_timer.init(freq=5, mode=Timer.PERIODIC, callback=update_game)

    while True:
        if player.state == True:
            # If the snake is alive
            if up.value() == 0:
                    player.change_dir(Snake.up)
                    
            elif right.value() == 0:
                    player.change_dir(Snake.right)
                    
            elif left.value() == 0:
                    player.change_dir(Snake.left)
                    
            elif down.value() == 0:
                    player.change_dir(Snake.down)
        
        else:
            # If the snake is dead
            if up.value() == 0:
                # Revive our snake friend
                display.fill_rectangle(0, 0, 320, 240, 0) # fill whole screen with black
                player.reset()
                food = urandom.choice([coord for coord in VALID_RANGE if coord not in player.segments])
                display.fill_rectangle(food[0] * SEGMENT_PIXELS , food[1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, color565(255,255,255)) # white
                
                
def update_game(timer):
    global food
    global player
    
    # Remove the previous tail of the snake (more effecient than clearing the entire screen and redrawing everything)
    display.fill_rectangle(player.segments[0][0] * SEGMENT_PIXELS, player.segments[0][1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 0)
    
    # Move the snake
    player.move()
    
    if player.state == False:
        # I think he's dead now :/
        display.fill_rectangle(0, 0, 320, 240, 0) # fill whole screen with black
        display.draw_text(int(SCREEN_WIDTH/2) - int(len("Game Over!")/2 * 8), int(SCREEN_HEIGHT/2) - 8, "Game Over!", arcadepix, color565(255,0,0))
        display.draw_text(
            int(SCREEN_WIDTH/2) - int(len("Snake length:" + str(len(player.segments))) /2 * 8), 
            int(SCREEN_HEIGHT/2) + 16, 
            "Snake length:" + str(len(player.segments)),
            arcadepix,
            color565(255,0,0))
        
    else:
        # Our snake is still alive and moving
        if food[0] == player.x and food[1] == player.y:
            # Our snake reached the food
            player.eat()
            food = urandom.choice([coord for coord in VALID_RANGE if coord not in player.segments])
            display.fill_rectangle(food[0] * SEGMENT_PIXELS , food[1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 1)
        
        player.draw()
        
    # Show the new frame
    # oled.show()

if __name__ == "__main__":
    main()