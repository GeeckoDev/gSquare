// 
// gSquare by Geecko
//
// This work is licensed under the Creative Commons BY-NC-SA 3.0 Unported License.
// See LICENSE for more details.

#include <pspkernel.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "game.h"
#include "controls.h"
#include "disp.h"
#include "level.h"
#include "audio.h"
#include "lua.h"
#include "config.h"
#include "language.h"

Game game = {S,0,0.f,0.f,0,0,{0},0,NULL};
Pause pause = {0};
Menu menu = {0,0,0,0,0,0.f,0.f,0.f,70.f,500.f,350.f};
State p_state;
static time_t old_time = 0;

// Game States

void initGameState()
{
  setMusic("audio/c418-sometimesimakevideogamemusic.mp3");
  #ifdef DEBUG
    setGameState(MENU);
  #else
    setGameState(GSQUARE);
  #endif
}


void pushGameState(int state)
{
  if (game.state_stack_size >= GAMESTATE_STACK_SIZE) return;
  if (getGameState() == state) return;
  
  switch (state)
  {
    case DEATH:
      playSound("death");
    break;
  }
  
  game.state_stack[game.state_stack_size++] = state;
}


void popGameState()
{
  if (game.state_stack_size <= 0) return;
  game.state_stack_size--;
}


void setGameState(int state)
{
  game.state_stack_size = 0;
  pushGameState(state);
}


int checkGameState(int state)
{
  int i;
  for (i=0; i!=game.state_stack_size; i++)
  {
    if (game.state_stack[i] == state) return 1;
  }
  return 0;
}


int getGameState()
{
  if (game.state_stack_size <= 0) return -1;
  return game.state_stack[game.state_stack_size-1];
}

// Player states

void setPlayerState(float x, float y, int g_dir)
{
  p_state.p_x = x;
  p_state.p_y = y;
  p_state.g_dir = g_dir;
}


void resetPlayerState()
{
  if (lvl.obj_nbr == 0) return;
  P_OBJ.vx = 0.f;
  P_OBJ.vy = 0.f;
  P_OBJ.x = p_state.p_x;
  P_OBJ.y = p_state.p_y;
  game.g_dir = p_state.g_dir;
}


void gameControls()
{
  updateControls();
  
  if (lvl.obj_nbr != 0 && getGameState() == INGAME)
  {
    // Change gravity dir
    if (!game.g_lock)
    {
      int g_inc = buttonJustPressed(PSP_CTRL_LTRIGGER) -
                  buttonJustPressed(PSP_CTRL_RTRIGGER) +
                  2 * buttonJustPressed(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER);
      game.g_dir += g_inc;
    }
    setCameraRot(180 + game.g_dir * 90);
    int g_dir_mod = game.g_dir % 4;
    while (g_dir_mod < 0) g_dir_mod += 4;
    game.g_x = (g_dir_mod == E) - (g_dir_mod == W);
    game.g_y = (g_dir_mod == S) - (g_dir_mod == N);
    
    // Square move
    float dir = buttonPressed(PSP_CTRL_RIGHT) - buttonPressed(PSP_CTRL_LEFT);
    if (getPad()->Lx < 127-15 || getPad()->Lx > 127+15)
    {
      dir = (getPad()->Lx - 127) / 127.f;
    }
    P_OBJ.vx += game.g_y * P_ACCEL * dir;
    P_OBJ.vy -= game.g_x * P_ACCEL * dir;
                                   
    // Square jump
    if (buttonPressed(PSP_CTRL_CROSS))
    {
      // Impulse
      if (game.g_x) P_OBJ.vx += OBJ_JUMP * (-P_OBJ.collide_x);
      if (game.g_y) P_OBJ.vy += OBJ_JUMP * (-P_OBJ.collide_y);
      // Jump higher by holding cross button
      if ((P_OBJ.vx > 0 && game.g_x < 0) ||
          (P_OBJ.vx < 0 && game.g_x > 0))
        P_OBJ.vx += OBJ_CST_JUMP * (-game.g_x);
      if ((P_OBJ.vy > 0 && game.g_y < 0) ||
          (P_OBJ.vy < 0 && game.g_y > 0))
        P_OBJ.vy += OBJ_CST_JUMP * (-game.g_y);
    }
  }
  
  #ifdef DEBUG
    if (buttonJustPressed(PSP_CTRL_SQUARE))
    {
      resetLevel();
    }
    if (buttonJustPressed(PSP_CTRL_TRIANGLE))
    {
      nextLevel();
    }
  #endif
  
  // Game state reset
  switch (getGameState())
  {
    case INGAME:
      if (buttonJustPressed(PSP_CTRL_START))
      {
        pause.i = 0;
        pushGameState(PAUSE);
      }
    break;
    case OUT_OF_BOUNDS:
      if (buttonJustPressed(PSP_CTRL_START))
      {
        cam.active = 1;
        popGameState();
        resetPlayerState();
      }
    break;
    case TIME_OVER:
      if (buttonJustPressed(PSP_CTRL_START))
      {
        cam.active = 1;
        popGameState();
        resetLevel();
      }
    break;
    case WIN:
      if (buttonJustPressed(PSP_CTRL_START))
      {
        save();
        waitFadeDone(&ui_fade);
        cam.active = 1;
        game.g_lock = 0;
        nextLevel();
        if (exit_state == 1) break;
        setGameState(INGAME);
        ui_fade.alpha = 255;
        if (lvl.obj_nbr != 0)
        {
          cam.x = cam.x_target = P_OBJ.x + P_OBJ.w/2.f;
          cam.y = cam.y_target = P_OBJ.y + P_OBJ.h/2.f;
        }
      }
    break;
    case DEATH:
      if (lvl.obj_nbr == 0) break;
      P_OBJ.state = 1;
      if (buttonJustPressed(PSP_CTRL_START))
      {
        cam.active = 1;
        popGameState();
        resetPlayerState();
        P_OBJ.state = 0;
      }
    break;
    case PAUSE:
      pause.i += buttonJustPressed(PSP_CTRL_UP) -
                 buttonJustPressed(PSP_CTRL_DOWN);
      if (pause.i < 0) pause.i += PAUSE_CHOICE_NBR;
      else if (pause.i >= PAUSE_CHOICE_NBR) pause.i -= PAUSE_CHOICE_NBR;
      
      if (buttonJustPressed(PSP_CTRL_START))
      {
        popGameState();
        if (pause.i == 1) resetPlayerState();
        if (pause.i == 2) resetLevel();
        if (pause.i == 3) {setGameState(MENU); menu.state = 0;}
      }
     break;
  }
}


void timer()
{
  time_t actual_time = time(NULL);
  
  if (actual_time >= old_time + 1)
  {
    game.time_elapsed--;
    if (game.time_elapsed < 15)
      setTimerAspect(1.5f,RED);
    old_time = actual_time;    
  }
  
  if (game.time_elapsed <= 0)
  {
    pushGameState(TIME_OVER);
    cam.active = 0;
    setMusic("!");
  }
}


void checkBounds()
{
  if (lvl.obj_nbr == 0) return;

  // Check if the player is "out of bounds"
  if (P_OBJ.x < lvl.limit_x0 ||
      P_OBJ.x+P_OBJ.type->tex_w > lvl.limit_x1 ||
      P_OBJ.y < lvl.limit_y0 ||
      P_OBJ.y+P_OBJ.type->tex_h > lvl.limit_y1)
  {
    cam.active = 0;
    pushGameState(OUT_OF_BOUNDS);
  }
}


void gamegSquare()
{
  setFadeMode(&main_fade,FADE_OUT,0);
  waitFadeDone(&main_fade);

  sceKernelDelayThread(3000000);

  setFadeMode(&main_fade,FADE_IN,0);
  waitFadeDone(&main_fade);

  setGameState(BANNER);
}


void gameBANNER()
{
  setFadeMode(&main_fade,FADE_OUT,0);
  waitFadeDone(&main_fade);

  sceKernelDelayThread(3000000);

  setFadeMode(&main_fade,FADE_IN,0);
  waitFadeDone(&main_fade);

  setGameState(MENU);
  sceKernelDelayThread(50000);
  g2dTexFree(&img.banner);
}


void gameMenu()
{
  char saved_lvl[512], buffer[512];
  strcpy(saved_lvl,lvl.next);
  FILE* fp = fopen("save","r");
  if (fp != NULL)
  {
    if (fscanf(fp,"%s",buffer) > 0) strcpy(saved_lvl,buffer);
    fclose(fp);
  }
  
  setFadeMode(&main_fade,FADE_OUT,0);
  waitFadeDone(&main_fade);

  int play = 0;
  while (!play)
  {
    updateControls();
    
    if (menu.state == 0) // Where Bluz can rotate
    {
      int i_inc = buttonJustPressed(PSP_CTRL_LTRIGGER) -
                  buttonJustPressed(PSP_CTRL_RTRIGGER);
      menu.i += i_inc;
      menu.rot_target = -menu.i * 90;
      menu.mod_i = menu.i % MENU_TITLE_NBR;
      if (menu.mod_i < 0) menu.mod_i += MENU_TITLE_NBR;
      
      if (buttonJustPressed(PSP_CTRL_CROSS))
      {
        menu.sub_i = 0;
        
        if (menu.mod_i < 3) // Story, Options, Credits
        {
          menu.state = 1;
        }
        else // Exit
        {
          menu.state = 2;
          setMusic("!");
          setFadeMode(&main_fade,FADE_IN,false);
          waitFadeDone(&main_fade);
          sceKernelExitGame();
        }
      }
    }
    else if (menu.state == 1) // Bluz is on top
    {
      menu.sub_i += buttonJustPressed(PSP_CTRL_DOWN) -
                    buttonJustPressed(PSP_CTRL_UP);
      
      if (menu.mod_i == 0) // Story
      {
        if (menu.sub_i < 0) menu.sub_i = 2;
        if (menu.sub_i > 2) menu.sub_i = 0;
        
        if (buttonJustPressed(PSP_CTRL_CROSS))
        {
          play = 1;
          menu.state = 2;
          luaDoFile("start.lua");
          if (menu.sub_i == 0)
          {
            strncpy(lvl.next,saved_lvl,512);
          }
          if (menu.sub_i == 2)
          {
            strncpy(lvl.next,"select.lua",512);
          }
        }
        if (buttonJustPressed(PSP_CTRL_CIRCLE))
        {
          menu.state = 0;
        }
      }
      else if (menu.mod_i == 1) // Options
      {
        if (menu.sub_i < 0) menu.sub_i = 3;
        if (menu.sub_i > 3) menu.sub_i = 0;

        if (buttonJustPressed(PSP_CTRL_LEFT))
        {
          switch (menu.sub_i)
          {
            case 0:
              cfg.music_vol -= 25;
              if (cfg.music_vol < 0) cfg.music_vol = 0;
              sprintf(text.menu.config_setting[0],"%d%%",cfg.music_vol);
            break;
            case 1:
              cfg.sound_vol -= 25;
              if (cfg.sound_vol < 0) cfg.sound_vol = 0;
              sprintf(text.menu.config_setting[1],"%d%%",cfg.sound_vol);
            break;
            case 2:
              cfg.lang_id--;
              if (cfg.lang_id < 0) cfg.lang_id = lang_n-1;
              loadLanguage();
            break;
          }
        }
        if (buttonJustPressed(PSP_CTRL_RIGHT))
        {
          switch (menu.sub_i)
          {
            case 0:
              cfg.music_vol += 25;
              if (cfg.music_vol > 100) cfg.music_vol = 100;
              sprintf(text.menu.config_setting[0],"%d%%",cfg.music_vol);
            break;
            case 1:
              cfg.sound_vol += 25;
              if (cfg.sound_vol > 100) cfg.sound_vol = 100;
              sprintf(text.menu.config_setting[1],"%d%%",cfg.sound_vol);
            break;
            case 2:
              cfg.lang_id++;
              if (cfg.lang_id >= lang_n) cfg.lang_id = 0;
              loadLanguage();
            break;
          }
        }
        if (buttonJustPressed(PSP_CTRL_CIRCLE))
        {
          menu.state = 0;
          configSave();
        }
      }
      else if (menu.mod_i == 2) // Credits
      {
        if (buttonJustPressed(PSP_CTRL_CIRCLE))
        {
          menu.state = 0;
        }
      }
    }
    
    sceKernelDelayThread(1000);
  }

  main_fade.color = WHITE;
  setFadeMode(&main_fade,FADE_IN,0);
  waitFadeDone(&main_fade);

  nextLevel();
  if (lvl.next[0] == '!') return;
  setGameState(INGAME);
  
  setFadeMode(&main_fade,FADE_OUT,0);
  ui_fade.alpha = 0;
}


int gameThread(SceSize args, void *argp)
{
  sceIoChdir(cwd);
  initGameState();
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

  while (!exit_state)
  {
    if (checkGameState(MENU))
    {
      gameMenu();
    }
    if (checkGameState(GSQUARE))
    {
      gamegSquare();
    }
    if (checkGameState(BANNER))
    {
      gameBANNER();
    }    
    if (checkGameState(INGAME))
    {
      gameControls();
      if (getGameState() != PAUSE)
      {
        physics(); // and player controls
      }
      if (getGameState() == INGAME)
      {
        checkBounds();
        timer();
      }
    }

    sceDisplayWaitVblankStart();
  }
  
  if (exit_state != EXCEPTION)
  {
    sceKernelDelayThread(2000000);
    sceKernelExitGame();
  }
  
  return 0;
}


void initGame()
{
  // Start game thread
  SceUID thid = sceKernelCreateThread("game_thread",gameThread,0x11,0x10000,
                                      THREAD_ATTR_USER | THREAD_ATTR_VFPU,0);
  if (thid < 0)
  {
    throwException("Can't create the game thread\n");
  }
  if (sceKernelStartThread(thid,0,0))
  {
    throwException("Can't start the game thread\n");
  }
}

// EOF
