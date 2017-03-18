Karen Flores, [18.03.17 19:58]
/*
 *
 *  Created on: 9 de mar. de 2017
 *      Author: Karen Flores
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include "fsm.h"
#include <wiringPi.h>
#include "tmr.h"
#include "coffe_machine.h"
#include "wallet_machine.h"

#define GPIO_BUTTON_S 26 //cable rojo
#define GPIO_BUTTON_C 21 //cable marron
#define GPIO_BUTTON_COIN 20 // cable blanco

#define GPIO_LED 17 //cable naranja
#define GPIO_CUP 27 //cable amarillo
#define GPIO_COFFEE 22 //cable verde
#define GPIO_MILK 10 //cable azul
//#define GPIO_COIN 7 //I use GPIO_LED

#define CUP_TIME 250
#define COFFEE_TIME 3000
#define MILK_TIME 3000

#define COFFEE_PRICE 50

#define FLAG_BUTTON_S 0x01
#define FLAG_BUTTON_C 0x02
#define FLAG_BUTTON_COIN 0x04
#define FLAG_TIMER 0x08

static int flags = 0;
static int current_machine;
enum fsm_types { COFFEE_FSM, WALLET_FSM};
static int money;
static int coin;
struct timespec inicio, fin, resultado;
struct timeval next_activation;
int frame;

static void button_start_isr (void) { flags |= FLAG_BUTTON_S; }
static void button_cancell_isr (void) { flags |= FLAG_BUTTON_C; }
static void button_coin_isr (void) { flags |= FLAG_BUTTON_COIN; }
static void timer_isr (union sigval arg) { flags |= FLAG_TIMER; }

static int button_coin_pressed(fsm_t* this){
  return (flags & FLAG_BUTTON_COIN);
}
static int button_start_pressed (fsm_t* this) {
  int aux = (flags & FLAG_BUTTON_S);
  if(aux){
    if(money>= COFFEE_PRICE){
      printf("Enough money.Let´s start!\n");
      return 1;
    }
    printf("Insert more money, please.\n" );
    return 0;
  }
  return aux;
 }
static int button_cancell_pressed (fsm_t* this) {
  return (flags & FLAG_BUTTON_C); }

static int timer_finished (fsm_t* this) {
  printf("Time out!\n" );
  return (flags & FLAG_TIMER); }

static void cup (fsm_t* this){
  flags =0;
  current_machine = COFFEE_FSM;
  printf("State Cup,Current money: %d\n", money);
  money -= COFFEE_PRICE;
  digitalWrite (GPIO_LED, 0);
  digitalWrite (GPIO_CUP, 1);
  tmr_startms((tmr_t*)(this->user_data), CUP_TIME);
}

static void coffee (fsm_t* this)
{
  flags = 0;
  printf("State Coffee\n" );
  digitalWrite (GPIO_CUP, 0);
  digitalWrite (GPIO_COFFEE, 1);
  tmr_startms((tmr_t*)(this->user_data), COFFEE_TIME);

}

static void milk (fsm_t* this)
{
  flags = 0;
  printf("State Milk\n" );
  digitalWrite (GPIO_COFFEE, 0);
  digitalWrite (GPIO_MILK, 1);
  tmr_startms((tmr_t*)(this->user_data), MILK_TIME);
}

static void finish (fsm_t* this)
{
  flags = 0;
  printf("State Finish\n" );
  digitalWrite (GPIO_MILK, 0);
  digitalWrite (GPIO_LED, 1);
  current_machine = WALLET_FSM;
  printf("The exchange is: %d\n", money);
  money = 0;
  printf("Thank you!\n" );
}

static void add_money (fsm_t* this){
  flags = 0;
  money = 50;
  printf("Total money: %d\n", money);
}
static void return_money (fsm_t* this){
  flags = 0;
  printf("Operation Cancelled, your money is: %d\n", money);
  money = 0;
}
// Explicit COFFEE FSM description
static fsm_trans_t cofm[] = {
  { COFM_WAITING, button_start_pressed, COFM_CUP,     cup    },
  { COFM_CUP,     timer_finished, COFM_COFFEE,  coffee },
  { COFM_COFFEE,  timer_finished, COFM_MILK,    milk   },
  { COFM_MILK,    timer_finished, COFM_WAITING, finish },
  {-1, NULL, -1, NULL },
};

// Explicit WALLET FSM description
static fsm_trans_t coinsm[] = {
  {WALLM_WAITING, button_coin_pressed, WALLM_WAITING, add_money},
  {WALLM_WAITING, button_cancell_pressed, WALLM_WAITING, return_money },
  {-1, NULL, -1, NULL },
};

// Utility functions, should be elsewhere
// res = a - b
void
timeval_sub (struct timespec *res, struct timespec *a, struct timespec *b)
{
  res->tv_sec = a->tv_sec - b->tv_sec;
  res->tv_nsec = a->tv_nsec - b->tv_nsec;
  if (res->tv_nsec < 0) {
    --res->tv_sec;
    res->tv_nsec += 1000000000;
  }
}

timeval_sub1 (struct timeval *res, struct timeval *a, struct timeval *b)
{
  res->tv_sec = a->tv_sec - b->tv_sec;
  res->tv_usec = a->tv_usec - b->tv_usec;
  if (res->tv_usec < 0) {
    --res->tv_sec;
    res->tv_usec +=

Karen Flores, [18.03.17 19:58]
1000000;
  }
}

// res = a + b
void
timeval_add (struct timeval *res, struct timeval *a, struct timeval *b)
{
  res->tv_sec = a->tv_sec + b->tv_sec
    + a->tv_usec / 1000000 + b->tv_usec / 1000000;
  res->tv_usec = a->tv_usec % 1000000 + b->tv_usec % 1000000;
}

// wait until next_activation (absolute time)
void delay_until (struct timeval* next_activation)
{
  struct timeval now, timeout;
  gettimeofday (&now, NULL);
  timeval_sub1 (&timeout, next_activation, &now);
  select (0, NULL, NULL, NULL, &timeout);
}


int main ()
{
  tmr_t* tmr = tmr_new(timer_isr); // we creat the timer
  fsm_t* cofm_fsm = fsm_new (COFM_WAITING,cofm, tmr);
  fsm_t* coinsm_fsm = fsm_new(WALLM_WAITING,coinsm, NULL);

  wiringPiSetupGpio();
  pinMode (GPIO_BUTTON_S, INPUT);
  pinMode (GPIO_BUTTON_C, INPUT);
  pullUpDnControl(GPIO_BUTTON_S, PUD_DOWN);
  pullUpDnControl(GPIO_BUTTON_C, PUD_DOWN);
  pullUpDnControl(GPIO_BUTTON_COIN, PUD_DOWN);
  wiringPiISR (GPIO_BUTTON_S, INT_EDGE_FALLING, button_start_isr);
  wiringPiISR (GPIO_BUTTON_C, INT_EDGE_FALLING, button_cancell_isr);
  wiringPiISR (GPIO_BUTTON_COIN, INT_EDGE_FALLING, button_coin_isr);
  pinMode (GPIO_CUP, OUTPUT);
  pinMode (GPIO_COFFEE, OUTPUT);
  pinMode (GPIO_MILK, OUTPUT);
  pinMode (GPIO_LED, OUTPUT);
  digitalWrite (GPIO_LED, 1);
  struct timeval hiperperiod;
  hiperperiod.tv_sec=0;
  hiperperiod.tv_usec=7000000;
  printf("Set up finished\n");

  gettimeofday (&next_activation, NULL);
  while (1) {
    switch (frame) {
      case 0: clock_gettime (CLOCK_MONOTONIC,&inicio);
              fsm_fire (cofm_fsm);
              fsm_fire (coinsm_fsm);
              clock_gettime (CLOCK_MONOTONIC,&fin);
              timeval_sub (&resultado,&fin , &inicio);
              delay_until (&next_activation);
    }
    frame = (frame + 1)%1;
    timeval_add(&next_activation, &next_activation, &hiperperiod);
        }
  return 0;
}
