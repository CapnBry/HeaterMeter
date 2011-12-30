// SPILCD - Extension of the ShiftRegister LCD class using the SPI bus
// as the shift register's data and clock, and using a 75HC595 latched
// shift register instead
// by Bryan Mayland
// QA - N/C
// QB - N/C
// QC - RS
// QD - E
// QE - D4
// QF - D5
// QG - D6
// QH - D7

#include "SPILCD.h"
#include "Arduino.h"

SPILCD::SPILCD(uint8_t srlatch, uint8_t lines) : ShiftRegLCD()
{
    pinMode(MOSI, OUTPUT); // Shift Register (Serial Input) Data
    pinMode(SCK, OUTPUT);  // Shift Register Clock
    //MISO pin automatically overrides to INPUT.
    //pinMode(MISO, INPUT);
    // SS must be set OUTPUT/HIGH. If it goes to INPUT/LOW,
    // the ATmega automatically switches to SPI slave mode
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    // SPI = clk/4
    SPCR = _BV(SPE) | _BV(MSTR);
    // SPI = clk/8
    //SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0) | _BV(SPI2X);
    // SPI = clk/16
    //SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
    // SPI = clk/128
    //SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0) | _BV(SPR1);
    // The enable line may have been left high by something so we need to clear it
    spi_byte(0);
    // Call init(), which calls virtual init4bits() what could possiblie
    // go wrong? It is ok in this instance because we are the most derived
    // class and the base default constructor doesn't call init()
    init(srlatch, 0, 0, lines, 0);
}

// Write a byte out SPI and toggle the Storage Register Clock
void SPILCD::spi_byte(uint8_t out)
{
    SPDR = out;
    while (!(SPSR & _BV(SPIF)))
        ;
    // Data needs to 40ns setup time and storage clock needs 16ns pulse
    // time but digitalWrite is really slow so the delay is built in
    digitalWrite(_srdata_pin, HIGH);
    digitalWrite(_srdata_pin, LOW);
}

void SPILCD::spi_lcd(uint8_t value)
{
    // The datasheet says that RS needs to be set-up 60ns before E goes
    // high but in my testing this caused the display to get wrecked
    //spi_byte(value);

    value |= SPI_LCD_E;
    spi_byte(value);
    // Enable needs to be HIGH for >450ns but sending a byte through SPI
    // at 8MHz takes 1uS so there's no need to wait

    value &= ~SPI_LCD_E;
    spi_byte(value);
}

void SPILCD::send(uint8_t value, uint8_t mode)
{
    uint8_t val;
    mode = mode ? SPI_LCD_RS : 0; // RS bit; LOW: command.  HIGH: character.
    
    val = mode | (value & 0xf0); //upper nibble
    spi_lcd(val);
    val = mode | (value << 4);    // lower nibble
    spi_lcd(val);

    delayMicroseconds(40);               // commands need > 37us to settle
}

void SPILCD::init4bits(uint8_t value)
{
    spi_lcd(value & 0xf0);
    delayMicroseconds(40);               // commands need > 37us to settle
}
