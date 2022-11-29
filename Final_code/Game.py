# PicoGame.py by YouMakeTech
# A class to easily write games for the Raspberry Pi Pico RetroGaming System
from machine import Pin, PWM, SPI, Timer
from ili9341n import ili9341
from framebuf import FrameBuffer, MONO_HLSB
import time
import random

class Game(ili9341):
    def __init__(self):
        self.SCREEN_WIDTH = 320
        self.SCREEN_HEIGHT = 240
        self.__up = Pin(9, Pin.IN, Pin.PULL_UP)
        self.__down = Pin(11, Pin.IN, Pin.PULL_UP)
        self.__left = Pin(8, Pin.IN, Pin.PULL_UP)
        self.__right = Pin(10, Pin.IN, Pin.PULL_UP)
        self.__buzzer = PWM(Pin(16))
        
        self.__spi = SPI(0,
          baudrate=40000000,
          polarity=1,
          phase=1,
          bits=8,
          firstbit=SPI.MSB,
          sck=Pin(6),
          mosi=Pin(7),
          miso=Pin(4))
        
        super().__init__(self.__spi, Pin(13), Pin(15),  Pin(14))
        
        self.__fb=[] # Array of FrameBuffer objects for sprites
        self.__w=[]
        self.__h=[]
    
    def center_text(self, s, color = 1):
        x = int(self.width/2)- int(len(s)/2 * 8)
        y = int(self.height/2) - 8
        self.text(s, x, y, color)
        
    def top_right_corner_text(self, s, color = 1):
        x = self.width - int(len(s) * 8)
        y = 0
        self.text(s, x, y, color)
        
    def add_sprite(self, buffer, w, h):
        fb = FrameBuffer(buffer, w, h, MONO_HLSB)
        self.__fb.append(fb)
        self.__w.append(w)
        self.__h.append(h)
       
    def sprite(self, n, x, y):
        self.blit(self.__fb[n], x, y,2)
        #self._writeblock(x, y, x + self.__w[n] - 1, y + self.__h[n] - 1, self.__fb[n])
        
    def sprite_width(self,n):
        return self.__w[n]
    
    def sprite_height(self,n):
        return self.__h[n]
        
    def button_up(self):
        return self.__up.value()==0
    
    def button_down(self):
        return self.__down.value()==0
    
    def button_left(self):
        return self.__left.value()==0
    
    def button_right(self):
        return self.__right.value()==0
    
    def any_button(self):
        # returns True if any button is pressed
        button_pressed=False
        if self.button_up():
            button_pressed = True
        if self.button_down():
            button_pressed = True
        if self.button_left():
            button_pressed = True
        if self.button_right():
            button_pressed = True
        return button_pressed
    
    def sound(self, freq, duty_u16 = 2000):
        # Make a sound at the selected frequency in Hz
        if freq>0:
            self.__buzzer.freq(freq)
            self.__buzzer.duty_u16(duty_u16)
        else:
            self.__buzzer.duty_u16(0)
   