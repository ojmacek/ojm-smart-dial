#pragma once
// Span-encoded line-art assets (formerly declared in the unused 2.2 MB TitanAssets.h).
struct AssetSpan { uint16_t offset; uint8_t length; uint8_t alpha; };
struct OutlineAsset { uint16_t width,height,count; const AssetSpan *spans; };
