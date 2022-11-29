from ili9341n import ili9341
from machine import Pin, SPI
from machine import Pin, PWM
import framebuf
from writer import CWriter
import freesans20
from Game import Game
import random
import time

game = Game()
wri = CWriter(game, freesans20)

first = "Welcome to Pico Retro Gaming"
second = "Please choose game:"

SCREEN_WIDTH=game.SCREEN_WIDTH                      # size of the screen
SCREEN_HEIGHT=game.SCREEN_HEIGHT


GAMELIST=["Pong","Space Invaders","Snake"]

current = 0
gameSelected = -1

while gameSelected < 0:
    game.fill(0)
    #game.text(second, int(SCREEN_WIDTH/2)-int(len(second)/2 * 8), 30, 6)
    CWriter.set_textpos(game, 20, 20)
    wri.setcolor(4,0)
    wri.printstring(first)
    CWriter.set_textpos(game, 50, 70)
    wri.setcolor(3,0)
    wri.printstring(second)
    for row in range(0, len(GAMELIST)):
        if row == current:
            game.rect(0, 100+row*25, SCREEN_WIDTH, 20, 1)
            color = 2
        else:
            color = 1
        
        #game.text(GAMELIST[row], int(SCREEN_WIDTH/2)-int(len(GAMELIST[row])/2 * 8), 100+row*8,color)
        CWriter.set_textpos(game, 100+row*25, int(SCREEN_WIDTH/2)-int(len(GAMELIST[row])/2 * 8) )
        wri.setcolor(color,0)
        wri.printstring(GAMELIST[row])
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
    elif (gameSelected == 2):
        execfile('snake.py')