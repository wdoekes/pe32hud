# Use a symlink from pe32hud.ino to pe32hud.cc (and not
# to the more common .cpp) because g++ and make will correctly guess
# their type, while the Arduino IDE does not open the .cpp file as well
# (it already has this file open as the ino file).
HEADERS = $(wildcard *.h bogoduino/*.h)
OBJECTS = pe32hud.o Device.o \
	  AirQualitySensorComponent.o DisplayComponent.o NetworkComponent.o \
	  SunscreenComponent.o TemperatureSensorComponent.o \
	  $(addsuffix .o, $(basename $(wildcard bogoduino/*.cpp))) \
	  $(addsuffix .o, $(basename $(wildcard local_bogoduino/*.cpp)))

# --- Arduino Uno AVR (8-bit RISC, by Atmel) ---
# /snap/arduino/current/hardware/arduino/avr/boards.txt:
# uno.build.mcu=atmega328p
# uno.build.f_cpu=16000000L
# uno.build.board=AVR_UNO
# uno.build.core=arduino
# uno.build.variant=standard
#CXX = /snap/arduino/current/hardware/tools/avr/bin/avr-gcc
#CPPFLAGS = \
#    -DARDUINO=10813 -DARDUINO_ARCH_AVR=1 -DARDUINO_AVR_UNO=1 \
#    -D__AVR_ATmega328P__ -DF_CPU=16000000  -D__ATTR_PROGMEM__=
#CXXFLAGS = \
#    -O \
#    -I/snap/arduino/current/hardware/tools/avr/avr/include \
#    -I/snap/arduino/current/hardware/arduino/avr/cores/arduino \
#    -I/snap/arduino/current/hardware/arduino/avr/variants/standard \

# --- NodeMcu ESP8266 Xtensa (32-bit RISC) ---
#CXX = $(HOME)/snap/arduino/current/.arduino15/packages/esp8266/tools/\
#xtensa-lx106-elf-gcc/2.5.0-4-b40a506/bin/xtensa-lx106-elf-gcc

# --- Test mode ---
CXX = g++
CPPFLAGS = -DTEST_BUILD -g -I./bogoduino -I./local_bogoduino \
	   -I../../libraries/Grove_-_LCD_RGB_Backlight \
	   -I../../libraries/DHT_sensor_library_for_ESPx
CXXFLAGS = -Wall -Os -fdata-sections -ffunction-sections
LDFLAGS = -Wl,--gc-sections # -s(trip)
ifeq ($(DEBUG),)
	LDFLAGS += -Wl,-s # strip
endif

test: ./pe32hud.test
	./pe32hud.test

clean:
	$(RM) $(OBJECTS) ./pe32hud.test

$(OBJECTS): $(HEADERS)

pe32hud.test: $(OBJECTS)
	$(LINK.cc) -o $@ $^
