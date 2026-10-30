#pragma once
// Local browse/edit state stays outside CAN v2 (V6.0.6 Halo 2).
//  - menu: a turn changes the page at once;
//  - click: choose a mode. A turn moves only the candidate; it is sent (requested -> CAN) after 250 ms of rest,
//    so a fast spin never sends the modes in between. A click sends a pending candidate at once and leaves;
//    a long press leaves without sending it; 8 s without input leaves on its own.
//  - V6.0.6.7: a long press in the menu runs the quick action (Launch / Boost, TorusContext.h); while Launch is
//    armed the next turn or click only cancels it. Options the situation locks are the ends of the selector
//    (they are always first or last), so the knob stops before them with the usual end bounce.
// Remote page commands select the page without unexpectedly entering or leaving local edit mode.
static constexpr uint32_t NAV_COMMIT_MS=250,NAV_IDLE_EXIT_MS=8000;
static bool detail_open=false,edit_pending=false;
static uint8_t edit_candidate=0,edit_page=0,nav_seen_page=255;
static uint32_t nav_last_input=0,nav_turn_at=0,nav_edge_seq=0,nav_commit_seq=0;
static int8_t nav_page_dir=1,nav_edge_dir=1;
// V6.0.6.9: the end bounce hit a locked option (the UI shows it and why for a moment).
static uint32_t nav_lock_seq=0;static uint8_t nav_lock_page=0,nav_lock_pos=0;
// UI order is independent of the stable CAN values: RWS OFF=0, STABLE=1, AGILE=2, AUTO=3 are shown (since V6.0.6.7)
// as Off, Auto, Stable, Agile. Keep the protocol compatible with existing PC mirrors.
static uint8_t mode_position(uint8_t page,uint8_t value){return page==0?(value==3?1:value==0?0:value+1):value;}
static uint8_t mode_value(uint8_t page,uint8_t position){return page==0?(position==1?3:position==0?0:position-1):position;}
static uint8_t applied_position(uint8_t page){return mode_position(page,page_mode(page));}
static void nav_commit(){
  if(!edit_pending)return;
  edit_pending=false;
  if(torus_locked(edit_page,edit_candidate))return;  // locked meanwhile (the car got faster): nothing is sent
  if(edit_candidate!=applied_position(edit_page)){apply_requested(0x11+edit_page,mode_value(edit_page,edit_candidate));++nav_commit_seq;}
}
static void nav_open(){detail_open=true;edit_pending=false;edit_page=requested.page;edit_candidate=applied_position(edit_page);}
static void nav_close(bool send){if(send)nav_commit();edit_pending=false;detail_open=false;}
static void navigation_input(int16_t delta,ButtonEvent button){
  const uint32_t now=millis();
  if(delta || button!=BUTTON_NONE)nav_last_input=now;
  // A release and the first turn can arrive in the same UI poll. Apply the
  // click first so that turn acts on the newly selected browse/edit context.
  if(!detail_open && (delta || button!=BUTTON_NONE) && torus_cancel_input())return;
  if(button==BUTTON_SHORT){if(detail_open)nav_close(true);else nav_open();}
  if(button==BUTTON_LONG){if(detail_open)nav_close(false);else torus_long_press(now);} // In a selector hold is escape, never reset.
  if(!delta)return;
  if(detail_open){
    // Locked options sit at the ends: the range stops before them (a candidate already there may stay).
    int32_t lo=0,hi=3;while(lo<3 && torus_locked(edit_page,lo))++lo;while(hi>0 && torus_locked(edit_page,hi))--hi;
    lo=min(lo,(int32_t)edit_candidate);hi=max(hi,(int32_t)edit_candidate);
    const int32_t target=(int32_t)edit_candidate+delta,clamped=clamp_i32(target,lo,hi);
    if(target!=clamped){++nav_edge_seq;nav_edge_dir=delta>0?1:-1;
      const int32_t blocked=clamped+nav_edge_dir;
      if(blocked>=0 && blocked<=3 && torus_locked(edit_page,blocked)){++nav_lock_seq;nav_lock_page=edit_page;nav_lock_pos=blocked;}}
    if(clamped!=edit_candidate){edit_candidate=clamped;edit_pending=true;nav_turn_at=now;}
  }else{
    int32_t page=((int32_t)requested.page+delta)%3;
    nav_page_dir=delta>0?1:-1;nav_seen_page=page<0?page+3:page;
    apply_requested(0x10,nav_seen_page);
  }
}
static void navigation_service(uint32_t now){
  // A remote page change: send what was chosen on the old page, follow the new one (direction = shorter way).
  if(nav_seen_page!=requested.page){
    if(nav_seen_page!=255)nav_page_dir=((requested.page+3-nav_seen_page)%3)==1?1:-1;
    nav_seen_page=requested.page;
    if(detail_open){nav_commit();edit_page=requested.page;edit_candidate=applied_position(edit_page);}
  }
  if(!detail_open)return;
  if(edit_pending && (uint32_t)(now-nav_turn_at)>=NAV_COMMIT_MS)nav_commit();
  // Without a local candidate the selector mirrors remote mode changes.
  if(!edit_pending)edit_candidate=applied_position(edit_page);
  if((uint32_t)(now-nav_last_input)>=NAV_IDLE_EXIT_MS)nav_close(true);
}
