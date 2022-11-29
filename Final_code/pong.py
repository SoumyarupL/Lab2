from ili9341n import ili9341
from machine import Pin, SPI
from machine import Pin, PWM
import framebuf
from Game import Game
import random
import time

game = Game()

# Game parameters
SCREEN_WIDTH = game.SCREEN_WIDTH             # size of the screen
SCREEN_HEIGHT = game.SCREEN_HEIGHT
BALL_SIZE = int(SCREEN_WIDTH/64)         # size of the ball size in pixels
PADDLE_WIDTH = int(SCREEN_WIDTH/8)       # size of the paddle in pixels
PADDLE_HEIGHT = int(SCREEN_HEIGHT/32)
PADDLE_Y = SCREEN_HEIGHT-PADDLE_HEIGHT # Vertical position of the paddle

# variables
ballX = 64     # ball position in pixels
ballY = 0
ballVX = 5.0    # ball velocity along x in pixels per frame
ballVY = 5.0    # ball velocity along y in pixels per frame

paddleX = int(SCREEN_WIDTH/2) # paddle  position in pixels
paddleVX = 6                  # paddle velocity in pixels per frame

soundFreq = 400 # Sound frequency in Hz when the ball hits something
score = 0

while True:
    # move the paddle when a button is pressed
    if game.button_right():
        # right button pressed
        paddleX += paddleVX
        if paddleX + PADDLE_WIDTH > SCREEN_WIDTH:
            paddleX = SCREEN_WIDTH - PADDLE_WIDTH
    elif game.button_left():
        # left button pressed
        paddleX -= paddleVX
        if paddleX < 0:
            paddleX = 0
    
    # move the ball
    if abs(ballVX) < 1:
        # do not allow an infinite vertical trajectory for the ball
        ballVX = 1

    ballX = int(ballX + ballVX)
    ballY = int(ballY + ballVY)
    
    # collision detection
    collision=False
    if ballX < 0:
        # collision with the left edge of the screen 
        ballX = 0
        ballVX = -ballVX
        collision = True
    
    if ballX + BALL_SIZE > SCREEN_WIDTH:
        # collision with the right edge of the screen
        ballX = SCREEN_WIDTH-BALL_SIZE
        ballVX = -ballVX
        collision = True

    if ballY+BALL_SIZE>PADDLE_Y and ballX > paddleX-BALL_SIZE and ballX<paddleX+PADDLE_WIDTH+BALL_SIZE:
        # collision with the paddle
        # => change ball direction
        ballVY = -ballVY
        ballY = PADDLE_Y-BALL_SIZE
        # increase speed!
        ballVY -= 0.2
        ballVX += (ballX - (paddleX + PADDLE_WIDTH/2))/10
        collision = True
        score += 10
        
    if ballY < 0:
        # collision with the top of the screen
        ballY = 0
        ballVY = -ballVY
        collision = True
        
    if ballY + BALL_SIZE > SCREEN_HEIGHT:
        # collision with the bottom of the screen
        # => Display Game Over
        game.fill(0)
        game.text("GAME OVER", int(SCREEN_WIDTH/2)-int(len("Game Over!")/2 * 8), int(SCREEN_HEIGHT/2) - 8)
        game.text(str(score), SCREEN_WIDTH-int(len(str(score))*8), 0)
        game.show()
        # play an ugly sound
        game.sound(200)
        time.sleep(0.5)
        game.sound(200,0)
        # wait for a button
        while game.button_right()!=1 and game.button_left()!=1:
            time.sleep(0.001)
        # game initialization
        ballX = 64
        ballY = 0
        ballVX = 5.0
        ballVY = 5.0
        paddleX = int(SCREEN_WIDTH/2)
        collision = False
        score = 0

    # Make a sound if the ball hits something
    # Alternate between 2 sounds
    if collision:
        if soundFreq==400:
            soundFreq=800
        else:
            soundFreq=400
    
        game.sound(soundFreq)
    
    # clear the screen
    game.fill(0)
    
    # display the paddle
    game.fill_rect(paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, 1)
    
    # display the ball
    game.fill_rect(ballX, ballY, BALL_SIZE, BALL_SIZE, 1)
    
    # display the score
    game.text(str(score), SCREEN_WIDTH-int(len(str(score))*8), 0)
    
    game.show()
    
    time.sleep(0.001)
    game.sound(200,0)