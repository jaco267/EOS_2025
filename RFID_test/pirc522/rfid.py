import logging
import threading
import time

logger = logging.getLogger(__name__)

RASPBERRY = object()
board = RASPBERRY

# Try with Raspberry PI imports first
import spidev
import RPi.GPIO as GPIO
SPIClass = spidev.SpiDev
def_pin_rst = 22
def_pin_irq = 18
def_pin_mode = GPIO.BOARD  #*10
pin_rst = 22;

class RFID(object):
    pin_irq = 18
    # commands  
    mode_idle = 0x00; mode_transrec = 0x0C; mode_reset = 0x0F
    #*-----------
    act_anticl = 0x93
    reg_tx_control = 0x14
    length = 16
    antenna_gain = 0x04
    irq = threading.Event()
    def __init__(self, bus=0, device=0, speed=1000000, 
             pin_irq=def_pin_irq, pin_mode=def_pin_mode):
        print("lest  gogogogo")
        self.pin_rst = pin_rst
        self.pin_irq = pin_irq
        self.spi = SPIClass()
        self.spi.open(bus, device)
        self.spi.max_speed_hz = speed
        assert pin_mode is not None
        GPIO.setmode(pin_mode)
        GPIO.setup(pin_rst, GPIO.OUT)
        GPIO.output(pin_rst, 1)
        self.init()
    def init(self):
        self.reset()
        self.dev_write(0x2A, 0x8D)
        self.dev_write(0x2B, 0x3E)
        self.dev_write(0x2D, 30)
        self.dev_write(0x2C, 0)
        self.dev_write(0x15, 0x40)
        self.dev_write(0x11, 0x3D)
        self.dev_write(0x26, (self.antenna_gain<<4))
        self.set_antenna()  #**** this is essential
    def set_antenna(self):  #*----used----
        current = self.dev_read(self.reg_tx_control)
        if ~(current & 0x03):  #* this is essential
            self.set_bitmask(self.reg_tx_control, 0x03)
    def set_bitmask(self, address, mask):
        current = self.dev_read(address)
        self.dev_write(address, current | mask)
    def spi_transfer(self, data):
        r = self.spi.xfer2(data)
        return r
    def dev_write(self, address, value):
        self.spi_transfer([(address << 1) & 0x7E, value])
    def dev_read(self, address):
        return self.spi_transfer([((address << 1) & 0x7E) | 0x80, 0])[1]

    def clear_bitmask(self, address, mask):  #*---used----
        current = self.dev_read(address)
        self.dev_write(address, current & (~mask))
    def card_write(self,  data):  #* ---used----
      back_data = []; back_length = 0
      error = False
      irq = 0x00;     irq_wait = 0x00
      last_bits = None
      n = 0
      irq = 0x77
      irq_wait = 0x30

      self.dev_write(0x02, irq | 0x80)
      self.clear_bitmask(0x04, 0x80)
      ##??? set_bitmask ----------
      self.set_bitmask(0x0A, 0x80)
      
      self.dev_write(0x01, self.mode_idle)
      ##???? REQA  
      for i in range(len(data)):
          self.dev_write(0x09, data[i])
      #**** mode tran
      self.dev_write(0x01, self.mode_transrec)
      self.set_bitmask(0x0D, 0x80)
      #****----------------
      i = 2000
      while True:
          n = self.dev_read(0x04)
          i -= 1
          if ~((i != 0) and ~(n & 0x01) and ~(n & irq_wait)):
              break
      self.clear_bitmask(0x0D, 0x80)
      if i != 0:
          if (self.dev_read(0x06) & 0x1B) == 0x00:
              error = False
              if n & irq & 0x01:
                logger.warning("Error E1"); error = True
              n = self.dev_read(0x0A)
              last_bits = self.dev_read(0x0C) & 0x07
              if last_bits != 0:
                  back_length = (n - 1) * 8 + last_bits
              else:
                  back_length = n * 8
              if n == 0:n = 1
              if n > self.length: n = self.length
              for i in range(n):
                  back_data.append(self.dev_read(0x09))
          else:
              logger.warning("Error E2")
              error = True

      return (error, back_data, back_length)
    def request(self):
        """Requests for tag. 
        Returns (False, None) if no tag is present, otherwise returns (True, tag type)
        """
        error = True
        back_bits = 0
        self.dev_write(0x0D, 0x07)
        (error, back_data, back_bits) = self.card_write( [0x26, ])  #0x26 req_mode
        if error or (back_bits != 0x10):
            return (True, None)
        return (False, back_bits)
    def anticoll(self):  #* this one is used
        """Anti-collision detection. Returns tuple of (error state, tag ID)."""
        back_data = []
        serial_number = []
        serial_number_check = 0
        self.dev_write(0x0D, 0x00)
        serial_number.append(self.act_anticl)
        serial_number.append(0x20)
        (error, back_data, back_bits) = self.card_write( serial_number)
        if not error:
            if len(back_data) == 5:  #* --- used
                for i in range(4):
                    serial_number_check = serial_number_check ^ back_data[i]

                if serial_number_check != back_data[4]:
                    error = True
            else:
                error = True
        return (error, back_data)

    def reset(self):
        self.dev_write(0x01, self.mode_reset)

    def cleanup(self):  #*----used
        """ cleanups GPIO."""
        GPIO.cleanup()