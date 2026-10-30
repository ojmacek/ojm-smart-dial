#pragma once
// Local browse/edit state stays outside CAN v2. Remote page commands select
// the page without unexpectedly entering or leaving local edit mode.
static bool detail_open=false;
static uint32_t nav_revision=0;
static int8_t nav_direction=0;
static bool nav_boundary=false,nav_confirm=false;
static void navigation_input(int16_t delta,ButtonEvent button){
  if(delta || button!=BUTTON_NONE){++nav_revision;nav_direction=delta>0?1:delta<0?-1:0;nav_boundary=false;nav_confirm=detail_open && button!=BUTTON_NONE;}
  if(delta){
    if(detail_open){
      uint8_t page=requested.page;
      nav_boundary=(page_mode(page)==0 && delta<0) || (page_mode(page)==3 && delta>0);
      apply_requested(0x11+page,clamp_i32((int32_t)page_mode(page)+delta,0,3));
    }else{
      int32_t page=((int32_t)requested.page+delta)%3;
      apply_requested(0x10,page<0?page+3:page);
    }
  }
  if(button==BUTTON_SHORT)detail_open=!detail_open;
  if(button==BUTTON_LONG)detail_open=false; // Hold is escape, never a hidden reset.
}
