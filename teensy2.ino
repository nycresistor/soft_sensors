#include <CapacitiveSensor.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 2     // which pin is your LED data line plugged into?
#define LED_COUNT 16  // how many LEDs are in the strip or ring you're driving?

class FifoBuffer {
private:
    static const int MAX_SIZE = 1000; // Maximum size of the buffer
    int buffer[MAX_SIZE]; // Array to store the buffer
    int front; // Index of the front element
    int rear; // Index of the rear element

public:
    FifoBuffer() {
        front = -1;
        rear = -1;
    }

    bool isEmpty() {
        return front == -1;
    }

    bool isFull() {
        return (rear + 1) % MAX_SIZE == front;
    }

    void push(int value) {
        if (isFull()) {
            // Buffer is full, overwrite the front element
            front = (front + 1) % MAX_SIZE;
        }

        rear = (rear + 1) % MAX_SIZE;
        buffer[rear] = value;
        if (front == -1) {
            front = rear;
        }
    }

    double calculateStandardDeviation() {
        if (isEmpty()) {
            return 0.0;
        }

        double sum = 0.0;
        int count = 0;
        int current = front;
        while (current != rear) {
            sum += buffer[current];
            current = (current + 1) % MAX_SIZE;
            count++;
        }
        sum += buffer[rear];
        count++;

        double mean = sum / count;

        double squaredSum = 0.0;
        current = front;
        while (current != rear) {
            double diff = buffer[current] - mean;
            squaredSum += diff * diff;
            current = (current + 1) % MAX_SIZE;
        }
        double diff = buffer[rear] - mean;
        squaredSum += diff * diff;

        double variance = squaredSum / count;
        double standardDeviation = sqrt(variance);

        return standardDeviation;
    }

    double calculateMean() {
        if (isEmpty()) {
            return 0.0;
        }

        double sum = 0.0;
        int count = 0;
        int current = front;
        while (current != rear) {
            sum += buffer[current];
            current = (current + 1) % MAX_SIZE;
            count++;
        }
        sum += buffer[rear];
        count++;

        double mean = sum / count;

        return mean;
    }
    
};

FifoBuffer buffer;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

CapacitiveSensor cs_4_2 = CapacitiveSensor(19,21);  // 19, 21 (teensy) or 8 11 (metro) 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.show();
  delay(2000);
  Serial.println("Autocalibration in progress...");
}

void lightup(int onoff, int red, int green, int blue) {
  // lightup the whole ring a given color, yes or no
  if (onoff == 0) {
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
  } else {
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, red, green, blue);
    }
  }
  strip.show();
}

void fraction_lightup(int meter) {
  // light up a fraction of the ring
  for (int i = 0; i < LED_COUNT; i++) {
    if (i < meter) {
      strip.setPixelColor(i, 3, 0, 12);
    } else {
      strip.setPixelColor(i, 0, 0, 0);
    }
  }
  strip.show();
}

void statecycle(int stateindex) {
  // rotate through preset states
  if (stateindex % 7 == 0) {
    lightup(1, 10, 0, 0);
  } else if (stateindex % 7 == 1) {
    lightup(1, 10, 2, 0);
  } else if (stateindex % 7 == 2) {
    lightup(1, 10, 8, 0);
  } else if (stateindex % 7 == 3) {
    lightup(1, 0, 10, 0);
  } else if (stateindex % 7 == 4) {
    lightup(1, 0, 0, 10);
  } else if (stateindex % 7 == 5) {
    lightup(1, 3, 0, 12);
  } else if (stateindex % 7 == 6) {
    lightup(1, 9, 0, 9);
  }
}

int activations = 0;
int cumulative = 0;
bool on = false;
bool off = true;
int onstreak = 0;
int offstreak = 0;
int sdcutoff = 20;
int transition_length = 1;

void loop() {
  // read the capacitive sensor
  long reading = cs_4_2.capacitiveSensor(30);

  // reset variables
  int trig = 0;

  if(!buffer.isFull()){
    // fill the buffer on startup; will take a few seconds to self-calibrate
    buffer.push(reading);
  } else {
    double mean = buffer.calculateMean();
    double sd = buffer.calculateStandardDeviation();
    if(abs(reading - mean)/sd < sdcutoff){
      // we're close to background; update the buffer
      buffer.push(reading);
      onstreak = 0;
      offstreak++;
      cumulative--;
      if(cumulative<0){cumulative = 0;}
    } else if ((reading - mean)/sd > sdcutoff){
      // we're detecting something interesting
      trig = (reading - mean)/sd - sdcutoff;
      onstreak++;
      cumulative++;
      if(cumulative>LED_COUNT){cumulative = LED_COUNT;}
      offstreak = 0;
    }

    // smooth out on versus off state management
    // opportunity to teach about buffering
    if(off && onstreak>=transition_length){
      off=false;
      on=true;
      activations += 1;
    } else if(on && offstreak>=transition_length){
      off = true;
      on = false;
    }

    // part 1: tap to state change
    // 1a. on / off
    lightup(int((activations%2)==1), 3,0,12);
    // 1b. state cycle
    //statecycle(activations);

    // part 2: charge / discharge
    //fraction_lightup(cumulative);

    // part 3: push to talk
    // 3a. contact; set sdcutoff to 20
    // 3b. proximity; set sdcutoff to 5
    //lightup(int(on), 3,0,12);

    Serial.print("Reading:  "); 
    Serial.print(reading);
    Serial.print("\t");
    Serial.print("Average:  ");
    Serial.print(mean);
    Serial.print("\t\t");
    Serial.print("Deviation:  ");
    Serial.print(sd);
    Serial.print("\n");

  }
  delay(5);  // arbitrary delay to limit data to serial port    
}
