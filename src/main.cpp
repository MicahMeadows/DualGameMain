#include "Pins.h"
#include "IRCommands.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#include <IRremote.hpp>
#include "GameState.h"
#include "GameMode.h"
#include "RouletteGame.h"

const int AF_TEST = 1;
const int AF_ROULETTE = 2;
const int AF_DUAL = 3;
const int AF_PEW = 4;
const int AF_CLICK = 5;

#include "DFRobotDFPlayerMini.h" // Include the DFRobot DFPlayer Mini library

#define FPSerial Serial1  // For ESP32, use hardware serial port 1

DFRobotDFPlayerMini mp3Player; // Create an instance of the DFRobotDFPlayerMini class

// IO Classes
Servo servo;

// Game State
GameState gameState = GameState::GS_SELECTING;
GameMode gameMode = GameMode::GM_ROULETTE;
bool gameWon = false;
RouletteGame rouletteGame;

// Game Details
const int sweepDelay = 2000;
int lastSweep = 0;
int gameResetMs = 5000;

void setupGameState()
{
  rouletteGame.state = RouletteState::RS_UNSTARTED;
  rouletteGame.bulletPosition = 0;
  rouletteGame.shotsFired = 0;
}

void setup()
{
  // delay to allow sound card to come online
  delay(2000);
  FPSerial.begin(9600, SERIAL_8N1, 16, 17); // Start serial communication for ESP32 with 9600 baud rate, 8 data bits, no parity, and 1 stop bit

  Serial.begin(115200);

  if (!mp3Player.begin(FPSerial))
  {
    Serial.println(F("Unable to begin:"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));
  
  mp3Player.volume(15);

  setupGameState();
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
}

void puncture()
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
    Serial.println("Chose GameMode: " + String((int)gameMode));
    return;
  }

  if (middleBtnState == LOW)
  {
    gameMode = static_cast<GameMode>((static_cast<int>(gameMode) + 1) % static_cast<int>(GameMode::GM_MAX));
    switch (gameMode) {
      case GameMode::GM_DUAL:
        mp3Player.play(AF_DUAL);
        Serial.println("Dual mode selected");
        break;
      case GameMode::GM_ROULETTE:
        mp3Player.play(AF_ROULETTE);
        Serial.println("Roulette mode selected");
        break;
    }

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
            puncture();
            IrReceiver.resume();
          }
          else if (IrReceiver.decodedIRData.command == BLUE_SHOOT_COMMAND)
          {
            showWinner(BLUE_LED_PIN, RED_LED_PIN);
            lastSweep = millis();
            puncture();
            IrReceiver.resume();
          }
          else
          {
            Serial.println("Invalid command: " + String(IrReceiver.decodedIRData.command));
            IrReceiver.resume();
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

void clearIRReceiver()
{
  IrReceiver.resume();
}

bool checkShootCommand()
{
  if (IrReceiver.decode())
  {
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)
    {
      IrReceiver.resume();
      return false;
    }
    if (IrReceiver.decodedIRData.protocol == UNKNOWN)
    {
      // Serial.println(F("Received noise or an unknown (or not yet enabled) protocol"));
      // IrReceiver.printIRResultRawFormatted(&Serial, true);
      IrReceiver.resume();
      return false;
    }
    else 
    {
      if (IrReceiver.decodedIRData.command == BLUE_SHOOT_COMMAND)
      {
        IrReceiver.resume();
        return true;
      }
      Serial.println("Invalid command: " + String(IrReceiver.decodedIRData.command));
      IrReceiver.resume();
    }
    Serial.println();
  }
  IrReceiver.resume();
  return false;
}

void playRoulette()
{
  switch (rouletteGame.state)
  {
    case RouletteState::RS_UNSTARTED:
      clearIRReceiver();
      rouletteGame.bulletPosition = random(1, 7);
      rouletteGame.shotsFired = 0;
      rouletteGame.state = RouletteState::RS_PLAYING;
      break;
    case RouletteState::RS_PLAYING:
    {
      unsigned long millisBefore = millis();
      if (checkShootCommand())
      {
        rouletteGame.shotsFired++;
        Serial.println("Shots fired: " + String(rouletteGame.shotsFired));
        Serial.println("Bullet position: " + String(rouletteGame.bulletPosition));
        bool bulletKills = rouletteGame.shotsFired == rouletteGame.bulletPosition;
        if (bulletKills)
        {
          rouletteGame.state = RouletteState::RS_GAME_OVER;
          mp3Player.play(AF_PEW);
          Serial.println("Game over!");
          puncture();
        }
        else
        {
          mp3Player.play(AF_CLICK);
          Serial.println("Took: " + String(millis() - millisBefore) + "ms");
          Serial.println("Safe!");
        }
        delay(1);
      }
      break;

    }
    case RouletteState::RS_GAME_OVER:
      rouletteGame.state = RouletteState::RS_UNSTARTED;
      gameState = GameState::GS_SELECTING;
      break;
  }
    
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