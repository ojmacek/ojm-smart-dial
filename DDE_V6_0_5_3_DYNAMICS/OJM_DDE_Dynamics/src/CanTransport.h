#pragma once
// V5.6 transport topology: driver/ISR on core 0, bounded RX, zero TX queue,
// single-shot, fixed mailboxes. No display or Serial calls from this task.
static portMUX_TYPE can_mailbox_mux=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t can_task_handle=nullptr;
static twai_message_t can_inbox[COMMAND_COUNT]={},can_ackbox[COMMAND_COUNT]={};
static uint8_t can_inbox_mask=0,can_ack_mask=0,can_snapshot[6]={DDE_MAGIC,0,3,1,0,0};
static uint32_t can_snapshot_revision=0,can_telemetry_revision=0,can_telemetry_at=0;
static Telemetry can_telemetry;
static uint32_t can_invalid_frames=0,can_complete_cycles=0;

static void can_publish_state(const RequestedState &requested,uint8_t faults) {
  const uint8_t next[6]={DDE_MAGIC,requested.page,requested.rws,requested.tv,requested.aero,faults};
  portENTER_CRITICAL(&can_mailbox_mux);
  if(memcmp(next,can_snapshot,6)!=0){memcpy(can_snapshot,next,6);++can_snapshot_revision;}
  portEXIT_CRITICAL(&can_mailbox_mux);
}
static void can_request_state() {
  portENTER_CRITICAL(&can_mailbox_mux);++can_snapshot_revision;portEXIT_CRITICAL(&can_mailbox_mux);
  if(can_task_handle) xTaskNotifyGive(can_task_handle);
}
static void can_queue_ack(const twai_message_t &command,uint8_t status) {
  if(command.data_length_code!=6 || command.data[0]<COMMAND_BASE || command.data[0]>=COMMAND_BASE+COMMAND_COUNT) return;
  twai_message_t ack={};ack.identifier=PROFILE.ack;ack.data_length_code=8;ack.ss=1;
  memcpy(ack.data,command.data,6);ack.data[6]=status;ack.data[7]=dde_checksum(ack.data,7);
  const uint8_t index=command.data[0]-COMMAND_BASE;
  portENTER_CRITICAL(&can_mailbox_mux);can_ackbox[index]=ack;can_ack_mask|=1U<<index;portEXIT_CRITICAL(&can_mailbox_mux);
  if(can_task_handle) xTaskNotifyGive(can_task_handle);
}
static uint8_t can_take_commands(twai_message_t *out) {
  uint8_t count=0;portENTER_CRITICAL(&can_mailbox_mux);
  for(uint8_t i=0;i<COMMAND_COUNT;++i) if(can_inbox_mask&(1U<<i)) out[count++]=can_inbox[i];
  can_inbox_mask=0;portEXIT_CRITICAL(&can_mailbox_mux);return count;
}
static bool can_take_telemetry(Telemetry &out,uint32_t &revision,uint32_t &received_at) {
  bool changed=false;portENTER_CRITICAL(&can_mailbox_mux);
  if(revision!=can_telemetry_revision) {out=can_telemetry;revision=can_telemetry_revision;received_at=can_telemetry_at;changed=true;}
  portEXIT_CRITICAL(&can_mailbox_mux);return changed;
}
static void can_transport_task(void *) {
  twai_general_config_t general=TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_GPIO,(gpio_num_t)CAN_RX_GPIO,TWAI_MODE_NORMAL);
  general.tx_queue_len=0;general.rx_queue_len=32;general.alerts_enabled=TWAI_ALERT_NONE;
  twai_timing_config_t timing=TWAI_TIMING_CONFIG_500KBITS();
  // Commands and ECU telemetry have separate IDs. Software rejects all others.
  twai_filter_config_t filter=TWAI_FILTER_CONFIG_ACCEPT_ALL();
  bool ready=twai_driver_install(&general,&timing,&filter)==ESP_OK;
  if(ready){gpio_set_pull_mode((gpio_num_t)CAN_RX_GPIO,GPIO_PULLUP_ONLY);ready=twai_start()==ESP_OK;}
  if(!ready) for(;;) ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
  uint32_t last_heartbeat=millis(),last_state=0,sent_revision=UINT32_MAX;
  uint32_t health_at=millis(),errors_before=0,stopped_at=0;
  uint8_t alive=0,ack_cursor=0;bool cooling=false;
  TelemetryAssembler assembler;
  for(;;){
    const uint32_t now=millis();twai_status_info_t health={};
    if(twai_get_status_info(&health)==ESP_OK){
      if(health.state==TWAI_STATE_BUS_OFF){
        if(twai_initiate_recovery()==ESP_OK){cooling=true;stopped_at=now;}
      }else if(health.state==TWAI_STATE_STOPPED){
        if(!cooling || (uint32_t)(now-stopped_at)>=1000){
          if(twai_start()==ESP_OK){cooling=false;errors_before=health.bus_error_count;health_at=now;}
        }
      }else if(health.state==TWAI_STATE_RUNNING){
        if((uint32_t)(now-health_at)>=100){
          const uint32_t new_errors=health.bus_error_count-errors_before;
          errors_before=health.bus_error_count;health_at=now;
          if(new_errors>=32 || health.tx_error_counter>=96 || health.rx_error_counter>=96){
            if(twai_stop()==ESP_OK){cooling=true;stopped_at=now;}
          }
        }
        if(!cooling){
          for(uint8_t n=0;n<32;++n){
            twai_message_t msg={};if(twai_receive(&msg,0)!=ESP_OK) break;
            if(msg.extd || msg.rtr) continue;
            if(msg.identifier==PROFILE.telemetry){
              if(msg.data_length_code!=8 || !dde_valid_fragment(msg.data)){++can_invalid_frames;continue;}
              Telemetry completed;
              if(assembler.accept(msg.data,now,completed)){
                portENTER_CRITICAL(&can_mailbox_mux);can_telemetry=completed;can_telemetry_at=now;++can_telemetry_revision;portEXIT_CRITICAL(&can_mailbox_mux);
                ++can_complete_cycles;
              }
              continue;
            }
            if(msg.identifier!=PROFILE.command || (msg.data_length_code!=2 && msg.data_length_code!=6) ||
                msg.data[0]<COMMAND_BASE || msg.data[0]>=COMMAND_BASE+COMMAND_COUNT) continue;
            if(!dde_valid_command(msg.data[0],msg.data[1])){can_queue_ack(msg,1);continue;}
            const uint8_t i=msg.data[0]-COMMAND_BASE;
            portENTER_CRITICAL(&can_mailbox_mux);can_inbox[i]=msg;can_inbox_mask|=1U<<i;portEXIT_CRITICAL(&can_mailbox_mux);
          }
          // Application ACKs have priority over event telemetry, never wait for redraw.
          bool submitted=false;
          for(uint8_t n=0;n<COMMAND_COUNT;++n){
            const uint8_t i=(ack_cursor+n)%COMMAND_COUNT;twai_message_t ack;
            portENTER_CRITICAL(&can_mailbox_mux);bool pending=(can_ack_mask&(1U<<i))!=0;ack=can_ackbox[i];portEXIT_CRITICAL(&can_mailbox_mux);
            if(!pending) continue;
            if(twai_transmit(&ack,0)==ESP_OK){
              submitted=true;portENTER_CRITICAL(&can_mailbox_mux);
              if(memcmp(can_ackbox[i].data,ack.data,8)==0) can_ack_mask&=~(1U<<i);
              portEXIT_CRITICAL(&can_mailbox_mux);ack_cursor=(i+1)%COMMAND_COUNT;
            }
            break;
          }
          if(!submitted){
            twai_message_t state={};state.identifier=PROFILE.state;state.data_length_code=8;state.ss=1;
            uint32_t revision;
            portENTER_CRITICAL(&can_mailbox_mux);memcpy(state.data,can_snapshot,6);revision=can_snapshot_revision;portEXIT_CRITICAL(&can_mailbox_mux);
            const bool heartbeat_due=(uint32_t)(now-last_heartbeat)>=HEARTBEAT_MS;
            const bool changed=revision!=sent_revision && (uint32_t)(now-last_state)>=8;
            if(heartbeat_due || changed){
              state.data[6]=alive;state.data[7]=dde_checksum(state.data,7);
              if(twai_transmit(&state,0)==ESP_OK){alive=(alive+1)&15;sent_revision=revision;last_state=now;if(heartbeat_due)last_heartbeat=now;}
            }
          }
        }
      }
    }
    ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(2)>0?pdMS_TO_TICKS(2):1);
  }
}
static bool can_begin(){return xTaskCreatePinnedToCore(can_transport_task,"can-transport",4096,nullptr,2,&can_task_handle,0)==pdPASS;}
