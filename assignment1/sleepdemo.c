#define F_CPU 8000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <util/delay.h>

/*port assinings */
#define LED_PORT PORTB
#define LED_DDR DDRB
#define LED_PIN PB0

#define BUZZER_PORT PORTB
#define BUZZER_DDR DDRB
#define BUZZER_PIN PB1

#define BUTTON_DDR DDRD
#define BUTTON_PORT PORTD
#define BUTTON_PIN PIND
#define BUTTON_BIT PD2 /* INT0 */

/* LCD pins */
#define LCD_DDR DDRC
#define LCD_PORT PORTC
#define LCD_RS PC0
#define LCD_E PC1

/*Inactivity timing */
#define INACTIVITY_MS (4000UL)
#define TICK_MS 250

/* 4-bit LCD driver */
static void lcd_pulse(void) {
  LCD_PORT |= (1 << LCD_E);
  _delay_us(1);
  LCD_PORT &= ~(1 << LCD_E);
  _delay_us(100);
}

static void lcd_nibble(uint8_t nibble) {
  LCD_PORT = (LCD_PORT & 0x03) | ((nibble & 0x0F) << 2);
  lcd_pulse();
}

static void lcd_write(uint8_t value, uint8_t is_data) {
  if (is_data)
    LCD_PORT |= (1 << LCD_RS);
  else
    LCD_PORT &= ~(1 << LCD_RS);
  lcd_nibble(value >> 4);
  lcd_nibble(value & 0x0F);
  _delay_us(50);
}

#define lcd_command(c) lcd_write((c), 0)
#define lcd_data(d) lcd_write((d), 1)

static void lcd_init(void) {
  LCD_DDR |= 0x3F; /* PC0..PC5 all outputs */
  _delay_ms(40);

  lcd_nibble(0x03);
  _delay_ms(5);
  lcd_nibble(0x03);
  _delay_us(150);
  lcd_nibble(0x03);
  lcd_nibble(0x02); /* switch to 4-bit mode */

  lcd_command(0x28); /* 2 line, 5x8 font */
  lcd_command(0x0C); /* display on, cursor off */
  lcd_command(0x06); /* auto-increment cursor */
  lcd_command(0x01); /* clear */
  _delay_ms(2);
}

static void lcd_clear(void) {
  lcd_command(0x01);
  _delay_ms(2);
}

static void lcd_goto(uint8_t row, uint8_t col) {
  uint8_t base;
  if (row == 0) {
    base = 0x80;
  } else {
    base = 0xC0;
  }
  lcd_command(base + col);
}

static void lcd_print(const char *s) {
  while (*s)
    lcd_data((uint8_t)*s++);
}

/*  interrupt setup */
static void gpio_init(void) {
  LED_DDR |= (1 << LED_PIN);
  LED_PORT &= ~(1 << LED_PIN);

  BUZZER_DDR |= (1 << BUZZER_PIN);
  BUZZER_PORT &= ~(1 << BUZZER_PIN);

  BUTTON_DDR &= ~(1 << BUTTON_BIT); // set button as input
  BUTTON_PORT |= (1 << BUTTON_BIT); // enable internal pullup resistor
}

static void int0_init(void) {
  EICRA &= ~((1 << ISC01) | (1 << ISC00)); /* rising edge enabled */
  EIMSK |= (1 << INT0);                    // enabling interupt 0
}

static void buzzer_beep(uint16_t ms) {
  BUZZER_PORT |= (1 << BUZZER_PIN);
  while (ms--)
    _delay_ms(1);
  BUZZER_PORT &= ~(1 << BUZZER_PIN);
}

ISR(INT0_vect) {} // interrupt service routine for interrupt zero

/*  Sleep configuration power down */
static void enter_power_down_sleep(void) {
  lcd_clear();
  lcd_goto(0, 0);
  lcd_print("SLEEPING...");
  LED_PORT &= ~(1 << LED_PIN); // disable led to show sleeping

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();           // ignore interrupts before the system goes to sleep
  sleep_enable();  // sleep enabling bit
  sei();           // enables read for interrupts
  sleep_cpu();     /* waits for external interrupt */
  sleep_disable(); /* resumes after interrupt */
}

static void wait_for_button_release(void) {
  while (!(BUTTON_PIN & (1 << BUTTON_BIT)))
    _delay_ms(10);
  _delay_ms(50); /* settle time */
}

int main(void) {
  gpio_init();
  int0_init();
  lcd_init();
  sei();

  lcd_clear();
  lcd_goto(0, 0);
  lcd_print("NORMAL OP");

  while (1) {
    lcd_goto(1, 0);
    lcd_print("sleepdemo      ");

    for (uint16_t elapsed = 0; elapsed < INACTIVITY_MS; elapsed += TICK_MS) {
      LED_PORT |=
          (1 << LED_PIN); /* lights continoulsly to show cpu is wroking */
      _delay_ms(TICK_MS);

      BUZZER_PORT |= (2 << BUZZER_PIN);
    }

    enter_power_down_sleep();

    buzzer_beep(151);
    lcd_clear();
    lcd_goto(1, 0);
    lcd_print("sleepdenied"); // message after sleep is interrupted

    wait_for_button_release();
  }
}

