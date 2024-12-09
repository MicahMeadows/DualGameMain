#include <Arduino.h>
#include <ESP32Servo.h>
#include <IRremote.hpp>
#include "GameState.h"
#include "GameMode.h"

GameState gameState = GameState::GS_SELECTING;
GameMode gameMode = GameMode::GM_ROULETTE;

const int SERVO_PIN = 23;
const int BTN_PIN = 22;
const int TEST_LED_PIN = 13;
const int RED_LED_PIN = 2;
const int BLUE_LED_PIN = 4;

const int RIGHT_BTN_PIN = 21;
const int MIDDLE_BTN_PIN = 32;
// const int LEFT_BTN_PIN = 32;

const uint8_t RED_SHOOT_COMMAND = 0x35;
const uint8_t BLUE_SHOOT_COMMAND = 0x7;
const uint8_t RESET_GAME_COMMAND = 0x36;

const int IR_RECV_PIN = 15;

const int sweepDelay = 2000;
int lastSweep = 0;

bool gameWon = false;

int gameResetMs = 5000;

int lastCommand = 0;

Servo servo;

void setup()
{
  // pinMode(LEFT_BTN_PIN, INPUT_PULLUP);
  pinMode(MIDDLE_BTN_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BTN_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(TEST_LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  servo.attach(SERVO_PIN);
  servo.write(0);

  IrReceiver.begin(IR_RECV_PIN, ENABLE_LED_FEEDBACK);

  printActiveIRProtocols(&Serial);

  Serial.begin(115200);
}

void sweep()
{
  servo.write(25);
  delay(1000);
  servo.write(0);
  delay(1000);
}

void resetGame()
{
  gameWon = false;
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
}

void showWinner(int winnerPin, int loserPin)
{
  gameWon = true;
  bool winnerBlue = winnerPin == BLUE_LED_PIN;
  if (winnerBlue)
  {
    Serial.println("Blue wins!");
  }
  else
  {
    Serial.println("Red wins!");
  }

  digitalWrite(winnerPin, HIGH);
  digitalWrite(loserPin, LOW);
}

void handleSelectingMode()
{
  int middleBtnState = digitalRead(MIDDLE_BTN_PIN);
  int rightBtnState = digitalRead(RIGHT_BTN_PIN);

  if (rightBtnState == LOW)
  {
    gameState = GameState::GS_SETTINGS;
    Serial.println("Chose GameMode:" + String((int)gameMode));
    return;
  }

  if (middleBtnState == LOW)
  {
    gameMode = static_cast<GameMode>((static_cast<int>(gameMode) + 1) % static_cast<int>(GameMode::GM_MAX));
    Serial.println("New mode: " + String((int)gameMode));
    delay(500);

  }
}

void handleSettingUpMode()
{
  gameState = GameState::GS_PLAYING;
}

void playDual()
{
  if (gameWon)
  {
    delay(gameResetMs);
    resetGame();
  }
  
  if (IrReceiver.decode())
  {
    if (IrReceiver.decodedIRData.protocol == UNKNOWN)
    {
      Serial.println(F("Received noise or an unknown (or not yet enabled) protocol"));
      IrReceiver.printIRResultRawFormatted(&Serial, true);
      IrReceiver.resume();
    }
    else 
    {
      if (!gameWon)
      {
        if (millis() - lastSweep > sweepDelay || lastSweep == 0)
        {
          if (IrReceiver.decodedIRData.command == RED_SHOOT_COMMAND)
          {
            showWinner(RED_LED_PIN, BLUE_LED_PIN);
            lastSweep = millis();
            sweep();
            IrReceiver.resume();
          }
          else if (IrReceiver.decodedIRData.command == BLUE_SHOOT_COMMAND)
          {
            showWinner(BLUE_LED_PIN, RED_LED_PIN);
            lastSweep = millis();
            sweep();
            IrReceiver.resume();
          }
          else
          {
            Serial.println("Invalid command: " + String(IrReceiver.decodedIRData.command));
          }
        }
      }
      else
      {
        if (IrReceiver.decodedIRData.command == RESET_GAME_COMMAND)
        {
          resetGame();
        }
      }
      IrReceiver.printIRResultShort(&Serial);
      IrReceiver.printIRSendUsage(&Serial);
    }
    Serial.println();
  }

}

void playRoulette()
{

}

void playMultiRoulette()
{

}

void handlePlayingMode()
{
  switch (gameMode)
  {
    case GameMode::GM_DUAL:
      playDual();
      break;
    case GameMode::GM_ROULETTE:
      playRoulette();
      break;
    case GameMode::GM_MULTI_ROULETTE:
      playMultiRoulette();
      break;
  }
  
}

void loop()
{
  switch (gameState)
  {
    case GameState::GS_SELECTING:
      handleSelectingMode();
      break;
    case GameState::GS_SETTINGS:
      handleSettingUpMode();
      break;
    case GameState::GS_PLAYING:
      handlePlayingMode();
      break;
  }

  
}