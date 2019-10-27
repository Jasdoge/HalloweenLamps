#include <arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>

#define SLEEP_1S 0b000110
#define SLEEP_2S 0b000111
#define SLEEP_4S 0b100000
#define SLEEP_8S 0b100001

#define adc_disable() ADCSRA &= ~ bit(ADEN) // disable ADC (before power-off)
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

const unsigned int WAKE_CYCLES = 10000;	// Multiply by 2 to get the total wake time. 10800 = 21600 sec = 6h
unsigned int cycle = 0;

byte tilt = 2;		// 0 = dark, 4 = light. Anything in between uses the off value

bool off = false;	// Tracks light is on state
const byte PIN_LAMP_A = PB0;
const byte PIN_LAMP_B = PB1;
const byte PIN_SENSOR_IN = A2;
const byte PIN_SENSOR_OUT = PB3;
const byte PIN_RNG = A1;

ISR(WDT_vect) {
	wdt_disable();  // disable watchdog
}

void sleep( byte duration = SLEEP_8S ){
	
	MCUSR = 0;                          // reset various flags
	WDTCR |= 0b00011000;               // see docs, set WDCE, WDE
	WDTCR =  0b01000000 | duration;    // set WDIE, and 4s delay
	wdt_reset();

	adc_disable();

	// Enters sleep mode
    sleep_enable();                          // enables the sleep bit in the mcucr register so sleep is possible
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // replaces above statement
	sleep_mode();

	adc_enable();

}


void setup(){

	pinMode(PIN_LAMP_A, OUTPUT);
	pinMode(PIN_LAMP_B, OUTPUT);
	pinMode(PIN_SENSOR_OUT, OUTPUT);

	pinMode(PIN_SENSOR_IN, INPUT);
	pinMode(PIN_RNG, INPUT);

}

void loop(){

	digitalWrite(PIN_SENSOR_OUT, HIGH);
	delayMicroseconds(100);
	const bool light = analogRead(PIN_SENSOR_IN) < 512;
	bool allowChange = false;

	digitalWrite(PIN_SENSOR_OUT, LOW);

	// Tip the scales. Switch is flipped when it's gone to 0 or 4
	if( !light && tilt > 0 ){

		--tilt;
		if( !tilt )
			off = false;

	}
	else if( light && tilt < 4 ){

		++tilt;
		if( tilt == 4 ){
			off = true;
			cycle = 0;		// Reset the cycle when it comes off due to brightness
		}

	}


	if( off || cycle > WAKE_CYCLES ){
		
		digitalWrite(PIN_LAMP_A, LOW);
		digitalWrite(PIN_LAMP_B, LOW);
		sleep(SLEEP_8S);
		return;

	}

	const byte ms = millis();
	unsigned int rng = (analogRead(PIN_RNG)+ms)%4;
	if( !rng ){

		byte lamps = ((ms+analogRead(PIN_RNG))%3)+1;

		byte n = (ms+analogRead(PIN_RNG))%50;
		for( byte i=0; i< 15+n; ++i ){

			bool r = (ms+analogRead(PIN_RNG))&1;
			if( lamps & 1 )
				digitalWrite(PIN_LAMP_A, r);
			r = (ms+analogRead(PIN_RNG))&1;
			if( lamps & 2 )
				digitalWrite(PIN_LAMP_B, r);

			delay(((ms+analogRead(PIN_RNG))%50)+1);

		}

	}

	digitalWrite(PIN_LAMP_A, HIGH);
	digitalWrite(PIN_LAMP_B, HIGH);

	sleep(SLEEP_2S);
	if( cycle <= WAKE_CYCLES )
		++cycle;


}
