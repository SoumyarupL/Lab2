from ili9341n import ili9341
from machine import Pin, SPI
from machine import Pin, PWM
import framebuf
from Game import Game
import random
import time

game = Game()

SCREEN_WIDTH=game.SCREEN_WIDTH                      # size of the screen
SCREEN_HEIGHT=game.SCREEN_HEIGHT

GAMELIST=["Pong","Space Invaders"]

current = 0
gameSelected = -1

while gameSelected < 0:
    game.fill(0)
    for row in range(0, len(GAMELIST)):
        if row == current:
            game.rect(0, row*8, SCREEN_WIDTH, 7, 1)
            color = 2
        else:
            color = 1
        
        game.text(GAMELIST[row], int(SCREEN_WIDTH/2)-int(len(GAMELIST[row])/2 * 8), row*8,color)
    
    game.show()
    
    time.sleep(0.2)
    
    buttonPressed = False
    
    while not buttonPressed:
        if (game.button_down()) and current < len(GAMELIST) - 1:
            current += 1
            buttonPressed = True
        elif (game.button_up() or game.button_left()) and current > 0:
            current -= 1
            buttonPressed = True
        elif game.button_right():
            buttonPressed = True
            gameSelected = current

    # Make a sound
    game.sound(1000)
    time.sleep(0.100)
    game.sound(1000,0)
    
# Start the selected game
if gameSelected >= 0:
    if (gameSelected == 0):
        execfile('pong.py')
    elif (gameSelected == 1):
        execfile('invaders.py')