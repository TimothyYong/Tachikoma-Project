/*
 * Copyright (C) 2014  RoboPeak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/*
 *  RoboPeak Lidar System
 *  Simple Data Grabber Demo App
 *
 *  Copyright 2009 - 2014 RoboPeak Team
 *  http://www.robopeak.com
 *
 *  An ultra simple app to fetech RPLIDAR data continuously....
 *
 */




#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <math.h>

#include "rplidar.h" //RPLIDAR standard sdk, all-in-one header

#ifndef _countof
#define _countof(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))
#endif

using namespace rp::standalone::rplidar;


bool checkRPLIDARHealth(RPlidarDriver * drv)
{
  u_result     op_result;
  rplidar_response_device_health_t healthinfo;


  op_result = drv->getHealth(healthinfo);
  if (IS_OK(op_result)) { // the macro IS_OK is the preperred way to judge whether the operation is succeed.
    printf("RPLidar health status : %d\n", healthinfo.status);
    if (healthinfo.status == RPLIDAR_STATUS_ERROR) {
      fprintf(stderr, "Error, rplidar internal error detected. Please reboot the device to retry.\n");
      // enable the following code if you want rplidar to be reboot by software
      // drv->reset();
      return false;
    } else {
      return true;
    }

  } else {
    fprintf(stderr, "Error, cannot retrieve the lidar health code: %x\n", op_result);
    return false;
  }
}

int main(int argc, const char * argv[]) {
  const char * opt_com_path = NULL;
  _u32         opt_com_baudrate = 115200;
  u_result     op_result;

  // read serial port from the command line...
  if (argc>1) opt_com_path = argv[1]; // or set to a fixed value: e.g. "com3" 

  // read baud rate from the command line if specified...
  if (argc>2) opt_com_baudrate = strtoul(argv[2], NULL, 10);


  if (!opt_com_path) {
#ifdef _WIN32
    // use default com port
    opt_com_path = "\\\\.\\com3";
#else
    opt_com_path = "/dev/ttyUSB0";
#endif
  }

  // create the driver instance
  RPlidarDriver * drv = RPlidarDriver::CreateDriver(RPlidarDriver::DRIVER_TYPE_SERIALPORT);

  if (!drv) {
    fprintf(stderr, "insufficent memory, exit\n");
    exit(-2);
  }


  // make connection...
  if (IS_FAIL(drv->connect(opt_com_path, opt_com_baudrate))) {
    fprintf(stderr, "Error, cannot bind to the specified serial port %s.\n"
        , opt_com_path);
    RPlidarDriver::DisposeDriver(drv);
    return 0;
  }



  // check health...
  if (!checkRPLIDARHealth(drv)) {
    RPlidarDriver::DisposeDriver(drv);
    return 0;
  }


  // start scan...
  drv->startScan();

  #define WINDOW_SIZE 800
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Surface *screen = SDL_SetVideoMode(WINDOW_SIZE, WINDOW_SIZE, 32, SDL_SWSURFACE);
  bool running = true;

  // fetch result and print it out...
  while (running) {
    rplidar_response_measurement_node_t nodes[360*2];
    size_t   count = _countof(nodes);

    op_result = drv->grabScanData(nodes, count);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
        break;
      }
    }

    SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0x00, 0x00, 0x00));
    uint32_t color = SDL_MapRGB(screen->format, 0x00, 0xFF, 0xFF);
    SDL_Rect rect;
    rect.w = 1;
    rect.h = 1;

    if (IS_OK(op_result)) {
      drv->ascendScanData(nodes, count);
      for (int pos = 0; pos < (int)count ; ++pos) {
        //               printf("%s theta: %03.2f Dist: %08.2f Q: %d \n", 
        //                   (nodes[pos].sync_quality & RPLIDAR_RESP_MEASUREMENT_SYNCBIT) ?"S ":"  ", 
        //                   (nodes[pos].angle_q6_checkbit >> RPLIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f,
        //                   nodes[pos].distance_q2/4.0f,
        //                   nodes[pos].sync_quality >> RPLIDAR_RESP_MEASUREMENT_QUALITY_SHIFT);
        double theta = (nodes[pos].angle_q6_checkbit >> RPLIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f + 180.0; // rotate 180 deg
        double distance = nodes[pos].distance_q2/20.0f;
        rect.x = (int)(distance * cos((double)theta * 3.1415926535 / 180.0)) + WINDOW_SIZE / 2;
        rect.y = (int)(distance * sin((double)theta * 3.1415926535 / 180.0)) + WINDOW_SIZE / 2;
        if (rect.x >= 0 && rect.x < WINDOW_SIZE && rect.y >= 0 && rect.y < WINDOW_SIZE)
          SDL_FillRect(screen, &rect, color);
        rect.x++;
        if (rect.x >= 0 && rect.x < WINDOW_SIZE && rect.y >= 0 && rect.y < WINDOW_SIZE)
          SDL_FillRect(screen, &rect, color);
        rect.x -= 2;
        if (rect.x >= 0 && rect.x < WINDOW_SIZE && rect.y >= 0 && rect.y < WINDOW_SIZE)
          SDL_FillRect(screen, &rect, color);
        rect.x++;
        rect.y++;
        if (rect.x >= 0 && rect.x < WINDOW_SIZE && rect.y >= 0 && rect.y < WINDOW_SIZE)
          SDL_FillRect(screen, &rect, color);
        rect.y -= 2;
        if (rect.x >= 0 && rect.x < WINDOW_SIZE && rect.y >= 0 && rect.y < WINDOW_SIZE)
          SDL_FillRect(screen, &rect, color);
        rect.y++;
      }
    }
    SDL_Flip(screen);
  }
  SDL_Quit();
  // done!
  RPlidarDriver::DisposeDriver(drv);
  return 0;
}
