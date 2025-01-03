// Libraries
#include <memory.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <IRremote.hpp>
#include <Deneyap_Hoparlor.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <esp_now.h>

// Definitions
#include "Pins.h"
#include "IRCommands.h"
#include "GameState.h"
#include "GameMode.h"
#include "RouletteGame.h"
#include "messages.h"

// Sounds
#include "sounds/shot2.h"
#include "sounds/revolver_click.h"
// #include "sounds/roulette.h"
// #include "sounds/dual.h"
// #include "sounds/play.h"
#include "sounds/revolver_cock.h"
#include "sounds/spin.h"
#include "sounds/cb_player.h"
#include "sounds/cb_one.h"
#include "sounds/cb_two.h"
#include "sounds/cb_three.h"
#include "sounds/cb_four.h"
#include "sounds/cb_pass.h"
#include "sounds/cb_roulette.h"
// #include "sounds/cb_five.h"
// #include "sounds/cb_six.h"

uint8_t testTowerMac[] = { 0x2C, 0xBC, 0xBB, 0x0D, 0xFB, 0x90 };
uint8_t gunMac[] = {0x4C, 0x11, 0xAE, 0x70, 0x51, 0x6C};

bool triggerBtnReceived = false;
bool safetyBtnReceived = false;
bool hammerBtnReceived = false;
unsigned long nextSafetyButtonAllowed = 0;
unsigned long safetyButtonModeDelay = 1500;

shoot_message shootMessage;
btn_press_message btnMessage;

esp_now_peer_info_t peerInfo;

class Speaker;

// IO Classes
Servo servo;

Speaker Speaker(DAC1, 0);

Wav Shot(shot2_wav);
Wav Click(revolver_click_wav);
// Wav Dual(dual_wav);
Wav RevolverCock(revolver_cock_wav);
Wav RevolverSpin(revolver_spin_wav);
Wav CowboyPlayer(raw_audio_cb_player_wav);
Wav CowboyOne(raw_audio_cb_one_wav);
Wav CowboyTwo(raw_audio_cb_two_wav);
Wav CowboyThree(raw_audio_cb_three_wav);
Wav CowboyFour(raw_audio_cb_four_wav);
std::shared_ptr<Wav> CowboyPass;
std::shared_ptr<Wav> Roulette;

void playRouletteAudio(void *params)
{
  if (!Roulette)
  {
    Roulette = std::make_shared<Wav>(cb_roulette_wav);
  }

  if (!Speaker.AlreadyPlaying(Roulette.get()))
  {
    Speaker.Play(Roulette.get());
  }

  vTaskDelay(700 / portTICK_PERIOD_MS);

  Roulette.reset();

  vTaskDelete(NULL);
}

void playPassRouletteAudio(void *params)
{
  if (!CowboyPass)
  {
    CowboyPass = std::make_shared<Wav>(raw_audio_cb_pass_wav);
  }
  if (!Roulette)
  {
    Roulette = std::make_shared<Wav>(cb_roulette_wav);
  }

  Speaker.Play(CowboyPass.get());
  vTaskDelay(200 / portTICK_PERIOD_MS);
  Speaker.Play(Roulette.get());
  vTaskDelay(700 / portTICK_PERIOD_MS);

  CowboyPass.reset();
  Roulette.reset();

  vTaskDelete(NULL);
}

// Game State
GameState gameState = GameState::GS_SELECTING;
GameMode gameMode = GameMode::GM_ROULETTE;
bool gameWon = false;
RouletteGame rouletteGame;

// Game Details
const int sweepDelay = 2000;
int lastSweep = 0;
int gameResetMs = 5000;
int servoResetTime = 0;

// If first loop iteration for selecting mode do things like saying speaker without btn press
bool selectModeFirstLoop = true;

unsigned long nextGameStartTime = 0; // time when next game can start. lets things like audio finish before replaying
int nextGameStartDelay = 3000;       // delay before next game can start after one finished.

void handleBtnPressMessage(btn_press_message message)
{
  Serial.print("Button press message received, button: ");
  Serial.print(btnMessage.button);
  Serial.print(", value: ");
  Serial.println(btnMessage.value);
  switch (message.button)
  {
  case 0:
    Serial.println("Trigger button pressed");
    triggerBtnReceived = true;
    break;
  case 1:
    Serial.println("Hammer button pressed");
    hammerBtnReceived = true;
    break;
  case 2:
    Serial.println("Safety button pressed");
    safetyBtnReceived = true;
    break;
  default:
    Serial.println("Unknown button");
    break;
  }
}

void OnDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  uint8_t type;
  memcpy(&type, incomingData, sizeof(uint8_t));

  Serial.print("message received of type: ");
  Serial.println(type);

  // shoot message
  switch (type)
  {
  case 1:
    triggerBtnReceived = true;
    memcpy(&shootMessage, incomingData, sizeof(shoot_message));
    break;
  case 2:
    memcpy(&btnMessage, incomingData, sizeof(btn_press_message));
    handleBtnPressMessage(btnMessage);
    break;

  default:
    Serial.println("Unknown message type");
  }
}

void setupRouletteGameState(int playerCount)
{
  
  rouletteGame.currentPlayer = 1;
  rouletteGame.chosePlayers = false;
  rouletteGame.state = RouletteState::RS_UNSTARTED;
  rouletteGame.playedIntro = false;
  rouletteGame.hammerLoaded = false;
  rouletteGame.playerCount = playerCount;

  rouletteGame.gun1.bulletPosition = random(1, 7);
  rouletteGame.gun2.bulletPosition = random(1, 7);
  rouletteGame.gun3.bulletPosition = random(1, 7);
  rouletteGame.gun4.bulletPosition = random(1, 7);
  rouletteGame.gun1.shotsFired = 0;
  rouletteGame.gun2.shotsFired = 0;
  rouletteGame.gun3.shotsFired = 0;
  rouletteGame.gun4.shotsFired = 0;

  rouletteGame.gun1.dead = false;

  // set all to dead then set players to alive
  rouletteGame.gun2.dead = true;
  rouletteGame.gun3.dead = true;
  rouletteGame.gun4.dead = true;

  if (rouletteGame.playerCount >= 2)
  {
    rouletteGame.gun2.dead = false;
  }
  if (rouletteGame.playerCount >= 3)
  {
    rouletteGame.gun3.dead = false;
  }
  if (rouletteGame.playerCount >= 4)
  {
    rouletteGame.gun4.dead = false;
  }
}

void setup()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_wifi_set_mac(WIFI_IF_STA, testTowerMac) == ESP_OK) {
    Serial.println("MAC address set successfully");
  } else {
    Serial.println("Failed to set MAC address");
  }

  delay(2000);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataReceived);

  RevolverCock.RepeatForever = false;
  RevolverSpin.RepeatForever = false;
  Shot.RepeatForever = false;
  Click.RepeatForever = false;
  // Dual.RepeatForever=false;
  CowboyPlayer.RepeatForever = false;
  CowboyOne.RepeatForever = false;
  CowboyTwo.RepeatForever = false;
  CowboyThree.RepeatForever = false;
  CowboyFour.RepeatForever = false;

  setupRouletteGameState(1);
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

void handleNewMode()
{
  switch (gameMode)
  {
  // case GameMode::GM_DUAL:
  //   // Speaker.Play(&Dual);
  //   Serial.println("Dual mode selected");
  //   break;
  case GameMode::GM_PASS_ROULETTE:
    xTaskCreate(
        playPassRouletteAudio,
        "Play 'Pass Roulette' audio",
        1000,
        NULL,
        1,
        NULL);
    Serial.println("Pass Roulette mode selected");
    break;
  case GameMode::GM_ROULETTE:
    xTaskCreate(
        // playPassRouletteAudio,
        playRouletteAudio,
        "Play 'Roulette' audio",
        1000,
        NULL,
        1,
        NULL);
    Serial.println("Roulette mode selected");
    break;
  }
}

void handleSelectingMode()
{
  if (selectModeFirstLoop)
  {
    handleNewMode();
    selectModeFirstLoop = false;
  }

  // int middleBtnState = digitalRead(MIDDLE_BTN_PIN);
  // int rightBtnState = digitalRead(RIGHT_BTN_PIN);

  // if (rightBtnState == LOW)
  if (triggerBtnReceived)
  {
    triggerBtnReceived = false;
    gameState = GameState::GS_SETTINGS;
    Serial.println("Chose GameMode: " + String((int)gameMode));
    return;
  }

  if (safetyBtnReceived && millis() > nextSafetyButtonAllowed)
  {
    safetyBtnReceived = false;
    gameMode = static_cast<GameMode>((static_cast<int>(gameMode) + 1) % static_cast<int>(GameMode::GM_MAX));

    handleNewMode();

    Serial.println("New mode: " + String((int)gameMode));
    nextSafetyButtonAllowed = millis() + safetyButtonModeDelay;
  }
}

void transitionToPlaying()
{
  triggerBtnReceived = false;
  gameState = GameState::GS_PLAYING;
}

void handleRouletteSettings(bool passRoulette)
{
  if (!rouletteGame.chosePlayers)
  {
    Speaker.Play(&CowboyPlayer);
    rouletteGame.chosePlayers = true;
  }
  if (safetyBtnReceived && millis() > nextSafetyButtonAllowed)
  {
    rouletteGame.playerCount += 1;
    if (rouletteGame.playerCount > 4)
    {
      rouletteGame.playerCount = 2;
    }
    switch (rouletteGame.playerCount)
    {
    case 1:
      Speaker.Play(&CowboyOne);
      break;
    case 2:
      Speaker.Play(&CowboyTwo);
      break;
    case 3:
      Speaker.Play(&CowboyThree);
      break;
    case 4:
      Speaker.Play(&CowboyFour);
      break;
    }
    nextSafetyButtonAllowed = millis() + safetyButtonModeDelay;
    safetyBtnReceived = false;
  }
  if (triggerBtnReceived)
  {
    if (!passRoulette || (passRoulette && rouletteGame.playerCount > 1))
    {
      transitionToPlaying();
    }
    triggerBtnReceived = false;
  }
}

void handleSettingUpMode()
{
  switch (gameMode)
  {
  case GameMode::GM_PASS_ROULETTE:
    handleRouletteSettings(true);
    break;
  case GameMode::GM_ROULETTE:
    transitionToPlaying();
    break;
  }
}

void handleGameOver()
{
  gameState = GameState::GS_GAME_OVER;
  nextGameStartTime = millis() + nextGameStartDelay;
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

bool checkWifiHammerLoadReceived()
{
  if (hammerBtnReceived)
  {
    hammerBtnReceived = false;
    return true;
  }
  return false;
}

bool checkWifiShotReceived()
{
  if (triggerBtnReceived)
  {
    triggerBtnReceived = false;
    return true;
  }
  return false;
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

void playRoulette(bool passRoulette = false)
{
  switch (rouletteGame.state)
  {
  case RouletteState::RS_UNSTARTED:
    triggerBtnReceived = false;
    hammerBtnReceived = false;
    safetyBtnReceived = false;
    if (passRoulette)
    {
      setupRouletteGameState(rouletteGame.playerCount);
    }
    else
    {
      setupRouletteGameState(1);
    }
    clearIRReceiver();
    // rouletteGame.bulletPosition = random(1, 7);
    // rouletteGame.bulletPosition = random(3, 7); // temp set to 3 as min
    // rouletteGame.shotsFired = 0;
    rouletteGame.state = RouletteState::RS_PLAYING;
    rouletteGame.playedIntro = false;
    rouletteGame.hammerLoaded = false;
    break;
  case RouletteState::RS_PLAYING:
  {
    if (rouletteGame.playedIntro == false)
    {
      delay(500);
      Speaker.Play(&RevolverSpin);
      // Speaker.Play(&PlaySound);
      // Speaker.Play(&RevolverCock);
      rouletteGame.playedIntro = true;
    }
    unsigned long millisBefore = millis();
    if (safetyBtnReceived)
    {
      if (!passRoulette)
      {
        rouletteGame.gun1.bulletPosition = random(1, 7);
        rouletteGame.gun1.shotsFired = 0;
        Speaker.Play(&RevolverSpin);
      }
      else
      {
        int startingPlayer = rouletteGame.currentPlayer;

        do
        {
            // Increment the current player
            rouletteGame.currentPlayer++;
            
            // Wrap around if exceeding player count
            if (rouletteGame.currentPlayer > rouletteGame.playerCount)
            {
                rouletteGame.currentPlayer = 1;
            }

            // Check if the current player is alive
            if (rouletteGame.currentPlayer == 1 && !rouletteGame.gun1.dead) break;
            if (rouletteGame.currentPlayer == 2 && !rouletteGame.gun2.dead) break;
            if (rouletteGame.currentPlayer == 3 && !rouletteGame.gun3.dead) break;
            if (rouletteGame.currentPlayer == 4 && !rouletteGame.gun4.dead) break;

        } while (rouletteGame.currentPlayer != startingPlayer);

        // Play the sound for the determined player
        switch (rouletteGame.currentPlayer)
        {
        case 1:
            Speaker.Play(&CowboyOne);
            break;
        case 2:
            Speaker.Play(&CowboyTwo);
            break;
        case 3:
            Speaker.Play(&CowboyThree);
            break;
        case 4:
            Speaker.Play(&CowboyFour);
            break;
        }
        
      }
      safetyBtnReceived = false;
    }
    if (!rouletteGame.hammerLoaded)
    {
      triggerBtnReceived = false;
      if (checkWifiHammerLoadReceived())
      {
        rouletteGame.hammerLoaded = true;
        Speaker.Play(&RevolverCock);
        hammerBtnReceived = false;
      }
    }
    else
    {

      if (checkWifiShotReceived())
      {
        RouletteGun *gun = nullptr;
        Serial.println("current player" + String(rouletteGame.currentPlayer) + " dead | " + String(rouletteGame.gun1.dead) + " | " + String(rouletteGame.gun2.dead) + " | " + String(rouletteGame.gun3.dead) + " | " + String(rouletteGame.gun4.dead));
        switch (rouletteGame.currentPlayer)
        {
        case 1:
          gun = &rouletteGame.gun1;
          break;
        case 2:
          gun = &rouletteGame.gun2;
          break;
        case 3:
          gun = &rouletteGame.gun3;
          break;
        case 4:
          gun = &rouletteGame.gun4;
          break;
        }
        if (gun != nullptr)
        {
          gun->shotsFired++;
          Serial.println("Shots fired: " + String(gun->shotsFired));
          Serial.println("Bullet position: " + String(gun->bulletPosition));
          if (!gun->dead)
          {
            bool bulletKills = gun->shotsFired == gun->bulletPosition;
            if (bulletKills)
            {
              gun->dead = true;
              int deadCount = 0;
              if (rouletteGame.gun1.dead)
              {
                deadCount++;
              }
              if (rouletteGame.gun2.dead)
              {
                deadCount++;
              }
              if (rouletteGame.gun3.dead && rouletteGame.playerCount >= 3)
              {
                deadCount++;
              }
              if (rouletteGame.gun4.dead && rouletteGame.playerCount >= 4)
              {
                deadCount++;
              }
              if (deadCount >= rouletteGame.playerCount - 1)
              {
                rouletteGame.state = RouletteState::RS_GAME_OVER;
                Serial.println("Game over!");
              }
              Speaker.Play(&Shot);
              // puncture();
              servo.write(25);
              hammerBtnReceived = false;
              servoResetTime = millis() + 1000;
              rouletteGame.hammerLoaded = false;
            }
            else
            {
              Serial.println("Took: " + String(millis() - millisBefore) + "ms");
              Serial.println("Safe!");
              Speaker.Play(&Click);
              hammerBtnReceived = false;
              rouletteGame.hammerLoaded = false;
            }
          }
        }
        delay(1);
      }
    }

    break;
  }
  case RouletteState::RS_GAME_OVER:
    rouletteGame.playerCount = 1;
    rouletteGame.state = RouletteState::RS_UNSTARTED;
    handleGameOver();
    break;
  }
}

void playPassRoulette()
{
  playRoulette(true);
}

void handlePlayingMode()
{
  switch (gameMode)
  {
  // case GameMode::GM_DUAL:
  //   playDual();
  //   break;
  case GameMode::GM_ROULETTE:
    playRoulette();
    break;
  case GameMode::GM_PASS_ROULETTE:
    playPassRoulette();
    break;
  }
}

void loop()
{
  if (millis() > servoResetTime)
  {
    servo.write(0);
  }
  Speaker.FillBuffer();
  switch (gameState)
  {
  case GameState::GS_SELECTING:
    handleSelectingMode();
    break;
  case GameState::GS_SETTINGS:
    // after move to settings from selecting reset selecting mode settings then handle settings
    selectModeFirstLoop = true;
    handleSettingUpMode();
    break;
  case GameState::GS_PLAYING:
    handlePlayingMode();
    break;
  case GameState::GS_GAME_OVER:
    if (millis() > nextGameStartTime)
    {
      gameState = GameState::GS_SELECTING;
    }
    break;
  }
}