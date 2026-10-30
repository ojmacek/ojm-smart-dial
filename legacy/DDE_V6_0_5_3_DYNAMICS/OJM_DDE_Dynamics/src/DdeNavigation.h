#pragma once
// Local browse/edit state stays outside CAN v2. Remote page commands select
// the page without unexpectedly entering or leaving local edit mode.
static bool detail_open=false;
static uint32_t nav_revision=0;
static int8_t nav_direction=0;
static bool nav_boundary=false,nav_confirm=false;
// UI order is independent of the stable CAN values: AUTO=3, STABLE=1,
// AGILE=2, OFF=0. Keep the protocol compatible with existing PC mirrors.
static uint8_t mode_position(uint8_t page,uint8_t value){return page==0?(value==3?0:value==0?3:value):value;}
static uint8_t mode_value(uint8_t page,uint8_t position){return mode_position(page,position);}
static void navigation_input(int16_t delta,ButtonEvent button){
  if(delta || button!=BUTTON_NONE){++nav_revision;nav_direction=delta>0?1:delta<0?-1:0;nav_boundary=false;nav_confirm=detail_open && button!=BUTTON_NONE;}
  if(delta){
    if(detail_open){
      uint8_t page=requested.page;
      uint8_t position=mode_position(page,page_mode(page));
      nav_boundary=(position==0 && delta<0) || (position==3 && delta>0);
      apply_requested(0x11+page,mode_value(page,clamp_i32((int32_t)position+delta,0,3)));
    }else{
      int32_t page=((int32_t)requested.page+delta)%3;
      apply_requested(0x10,page<0?page+3:page);
    }
  }
  if(button==BUTTON_SHORT)detail_open=!detail_open;
  if(button==BUTTON_LONG)detail_open=false; // Hold is escape, never a hidden reset.
}
