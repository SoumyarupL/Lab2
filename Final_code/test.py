from ili9341n import ili9341
from machine import Pin, SPI
from machine import Pin, PWM
from xglcd_font import XglcdFont
import framebuf
import random
import time


spi = SPI(0,
          baudrate=10000000,
          polarity=1,
          phase=1,
          bits=8,
          firstbit=SPI.MSB,
          sck=Pin(6),
          mosi=Pin(7),
          miso=Pin(4))

display = ili9341(spi, Pin(13), Pin(15),  Pin(14))

ALIEN2A_W = 11
ALIEN2A_H = 8
ALIEN2A = bytearray([0b00100000,0b10000000,
                     0b00010001,0b00000000,
                     0b00111111,0b10000000,
                     0b01101110,0b11000000,
                     0b11111111,0b11100000,
                     0b10111111,0b10100000,
                     0b10100000,0b10100000,
                     0b00011011,0b00000000])

buf = framebuf.FrameBuffer(ALIEN2A, 11, 8, framebuf.MONO_HLSB)
#game.add_sprite(buf, 240, 320)
display.fill(0)
s = "Hello World"
display.blit(buf,40,60)
display.text(s,100,100,1)
#display.fill(0)
#display.rect(100,100,50,50,1)
display.show()
