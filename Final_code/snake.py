from ili9341n import ili9341
from machine import Pin, SPI, Timer
from machine import Pin, PWM
import framebuf
from Game import Game
import urandom
import utime

game = Game()

game.fill(0)

SCREEN_HEIGHT = game.SCREEN_HEIGHT
SCREEN_WIDTH = game.SCREEN_WIDTH

SEGMENT_WIDTH = 24
SEGMENT_PIXELS = int(SCREEN_HEIGHT/SEGMENT_WIDTH)

SEGMENTS_HIGH = int(SCREEN_HEIGHT/SEGMENT_WIDTH)
SEGMENTS_WIDE = int(SCREEN_WIDTH/SEGMENT_WIDTH)
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
                game.sound(200)
                utime.sleep(0.5)
                game.sound(200,0)
            self.state = False
        
        self.x = new_x
        self.y = new_y
        
        self.segments[-1][0] = self.x
        self.segments[-1][1] = self.y
        
    def eat(self):
        game.fill_rect(self.x * SEGMENT_PIXELS, self.y * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 0)
        game.rect(self.x * SEGMENT_PIXELS, self.y * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 1)
        self.segments.append([self.x, self.y])
        # Make a sound
        game.sound(1000)
        utime.sleep(0.100)
        game.sound(1000,0)
        
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
        if new_y >= 24 or new_y < 0 or new_x >= 32 or new_x < 0 or [new_x, new_y] in self.segments:
            #print(new_x,new_y,SEGMENTS_HIGH,SEGMENTS_WIDE)
            return True
        else:
            return False
    
    def draw(self):
        game.rect(self.segments[-1][0] * SEGMENT_PIXELS, self.segments[-1][1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 1)


def main():
    global player
    global food
    
    player = Snake()
    food = urandom.choice([coord for coord in VALID_RANGE if coord not in player.segments])
    game.fill_rect(food[0] * SEGMENT_PIXELS , food[1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 1)
    
    # Playing around with this cool timer.
    game_timer.init(freq=5, mode=Timer.PERIODIC, callback=update_game)

    while True:
        if player.state == True:
            # If the snake is alive
            if game.button_up():
                    player.change_dir(Snake.up)
                    
            elif game.button_right():
                    player.change_dir(Snake.right)
                    
            elif game.button_left():
                    player.change_dir(Snake.left)
                    
            elif game.button_down():
                    player.change_dir(Snake.down)
        
        else:
            # If the snake is dead
            if game.button_up():
                # Revive our snake friend
                game.fill(0)
                player.reset()
                food = urandom.choice([coord for coord in VALID_RANGE if coord not in player.segments])
                game.fill_rect(food[0] * SEGMENT_PIXELS , food[1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 1)
                
                
def update_game(timer):
    global food
    global player
    
    # Remove the previous tail of the snake (more effecient than clearing the entire screen and redrawing everything)
    game.fill_rect(player.segments[0][0] * SEGMENT_PIXELS, player.segments[0][1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 0)
    
    # Move the snake
    player.move()
    
    if player.state == False:
        # I think he's dead now :/
        game.fill(0)
        game.text("Game Over!" , int(SCREEN_WIDTH/2) - int(len("Game Over!")/2 * 8), int(SCREEN_HEIGHT/2) - 8)
        game.text("Snake length:" + str(len(player.segments)) , int(SCREEN_WIDTH/2) - int(len("Snake length:" + str(len(player.segments))) /2 * 8), int(SCREEN_HEIGHT/2) + 16)
        
    else:
        # Our snake is still alive and moving
        if food[0] == player.x and food[1] == player.y:
            # Our snake reached the food
            player.eat()
            food = urandom.choice([coord for coord in VALID_RANGE if coord not in player.segments])
            game.fill_rect(food[0] * SEGMENT_PIXELS , food[1] * SEGMENT_PIXELS, SEGMENT_PIXELS, SEGMENT_PIXELS, 1)
        
        player.draw()
        
    # Show the new frame
    game.show()
    
while True:
    main()