// Include the arduino lib
#include <arduino.h>
// Include the libraries to handle sleep
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>

// Handy constants for setting sleep time
#define SLEEP_1S 0b000110
#define SLEEP_2S 0b000111
#define SLEEP_4S 0b100000
#define SLEEP_8S 0b100001

// Macros that let you toggle the analog to digital converter
#define adc_disable() ADCSRA &= ~ bit(ADEN) // disable ADC (before power-off)
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// NR sleep cycles before auto turning off.
const unsigned int WAKE_CYCLES = 10000;	// Multiply by 2 to get the total wake time. 10800 = 21600 sec = 6h
unsigned int cycle = 0;	// Current sleep cycle number

// This lets you "tilt" the reading from on to off. It's needed to check for multiple readings before turning on or off.
// Otherwise a car headlight might turn it on.
byte tilt = 2;		// 0 = dark, 4 = light. Anything in between uses the off value

bool off = false;	// Tracks light is on state

// Pin mapping
const byte PIN_LAMP_A = PB0;
const byte PIN_LAMP_B = PB1;
const byte PIN_SENSOR_IN = A2;
const byte PIN_SENSOR_OUT = PB3;
const byte PIN_RNG = A1;

// Needed to prevent crashing
ISR(WDT_vect) {
	wdt_disable();  // disable watchdog
}

// Enters sleep mode
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


// IT BEGINS
void setup(){

	// Setup pin mapping
	pinMode(PIN_LAMP_A, OUTPUT);
	pinMode(PIN_LAMP_B, OUTPUT);
	pinMode(PIN_SENSOR_OUT, OUTPUT);

	pinMode(PIN_SENSOR_IN, INPUT);
	pinMode(PIN_RNG, INPUT);

}

// This is where all logic takes place
void loop(){

	// Enable the sensor and wait a bit for it power up before reading
	digitalWrite(PIN_SENSOR_OUT, HIGH);
	delayMicroseconds(100);

	// Check if it's light outside
	const bool light = analogRead(PIN_SENSOR_IN) < 512;
	
	// Disable the sensor again to preserve power
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

	// If off, disable the lamps and sleep for 8 seconds
	if( off || cycle > WAKE_CYCLES ){
		
		digitalWrite(PIN_LAMP_A, LOW);
		digitalWrite(PIN_LAMP_B, LOW);
		sleep(SLEEP_8S);
		return;

	}

	// Arduino random is expensive, so we'll use milliseconds and a reading from an unconnected pin for RNG
	const byte ms = millis();
	unsigned int rng = (analogRead(PIN_RNG)+ms)%4;
	// 1/4 chance of flickering
	if( !rng ){

		// Pick what lamps to flicker. This is a bitwise value from 1 to 3
		byte lamps = ((ms+analogRead(PIN_RNG))%3)+1;

		// Shuffle max number of flickers
		byte n = (ms+analogRead(PIN_RNG))%50;
		for( byte i=0; i< 15+n; ++i ){

			// Randomize the lamp state
			bool r = (ms+analogRead(PIN_RNG))&1;
			if( lamps & 1 )
				digitalWrite(PIN_LAMP_A, r);
			r = (ms+analogRead(PIN_RNG))&1;
			if( lamps & 2 )
				digitalWrite(PIN_LAMP_B, r);

			// Wait a bit
			delay(((ms+analogRead(PIN_RNG))%50)+1);

		}

	}

	// Once flickering is done or not triggered, turn on the lights again
	digitalWrite(PIN_LAMP_A, HIGH);
	digitalWrite(PIN_LAMP_B, HIGH);

	// Wait 2 sec before rolling the flicker chance again
	sleep(SLEEP_2S);

	// Increment the wake tracker
	if( cycle <= WAKE_CYCLES )
		++cycle;


}
