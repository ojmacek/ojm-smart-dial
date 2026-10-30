#pragma once
// Pixel stream of one panel window (V6.0.6.2): the rectangle is read straight from the framebuffer row by row
// and converted to the panel's big-endian RGB565 in chunks. emit(buffer,pixels,first) sends one chunk; the two
// buffers alternate, so the next chunk is converted while the previous one is still on the wire (DMA).
// The rectangle is CO5300-aligned (even x and width), so pixels are converted two at a time.
static constexpr uint32_t QSPI_CHUNK_PIXELS=4096;
// src_rect: the rectangle's first pixel; stride: pixels from one row to the next (LCD_W in the frame, w in the pool).
template<class Emit> static void qspi_stream_rect(const uint16_t *src_rect,int16_t stride,int16_t w,int16_t h,uint16_t *const buffers[2],Emit &&emit){
 uint32_t filled=0;uint8_t which=0;bool first=true;uint16_t *buf=buffers[0];
 for(int16_t r=0;r<h;++r){
  const uint32_t *src=(const uint32_t*)(src_rect+(size_t)r*stride);int16_t c=0;
  while(c<w){
   const uint32_t n=min((uint32_t)(w-c),QSPI_CHUNK_PIXELS-filled);
   uint32_t *dst=(uint32_t*)(buf+filled);const uint32_t *from=src+c/2;
   for(uint32_t k=0;k<n/2;++k){const uint32_t v=from[k];dst[k]=((v&0x00FF00FFu)<<8)|((v>>8)&0x00FF00FFu);}
   filled+=n;c+=n;
   if(filled==QSPI_CHUNK_PIXELS){emit(buf,filled,first);first=false;which^=1;buf=buffers[which];filled=0;}
  }
 }
 if(filled)emit(buf,filled,first);
}
